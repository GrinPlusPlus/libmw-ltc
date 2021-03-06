#include <mw/node/CoinsView.h>
#include <mw/exceptions/ValidationException.h>
#include <mw/consensus/Aggregation.h>
#include <mw/common/Logger.h>

MW_NAMESPACE

std::vector<UTXO::CPtr> CoinsViewCache::GetUTXOs(const Commitment& commitment) const
{
    std::vector<UTXO::CPtr> utxos = m_pBase->GetUTXOs(commitment);

    const std::vector<CoinAction>& actions = m_pUpdates->GetActions(commitment);
    for (const CoinAction& action : actions) {
        if (action.pUTXO != nullptr) {
            utxos.push_back(action.pUTXO);
        } else {
            assert(!utxos.empty());
            utxos.pop_back();
        }
    }

    return utxos;
}

mw::BlockUndo::CPtr CoinsViewCache::ApplyBlock(const mw::Block::Ptr& pBlock)
{
    assert(pBlock != nullptr);

    auto pPreviousHeader = GetBestHeader();
    SetBestHeader(pBlock->GetHeader());

    std::for_each(
        pBlock->GetKernels().cbegin(), pBlock->GetKernels().cend(),
        [this](const Kernel& kernel) { m_pKernelMMR->Add(kernel); }
    );

    std::vector<UTXO> coinsSpent;
    std::for_each(
        pBlock->GetInputs().cbegin(), pBlock->GetInputs().cend(),
        [this, &coinsSpent](const Input& input) {
            UTXO spentUTXO = SpendUTXO(input.GetCommitment());
            coinsSpent.push_back(std::move(spentUTXO));
        }
    );

    std::vector<Commitment> coinsAdded;
    std::for_each(
        pBlock->GetOutputs().cbegin(), pBlock->GetOutputs().cend(),
        [this, &pBlock, &coinsAdded](const Output& output) {
            AddUTXO(pBlock->GetHeight(), output);
            coinsAdded.push_back(output.GetCommitment());
        }
    );

    ValidateMMRs(pBlock->GetHeader());

    return std::make_shared<mw::BlockUndo>(pPreviousHeader, std::move(coinsSpent), std::move(coinsAdded));
}

void CoinsViewCache::UndoBlock(const mw::BlockUndo::CPtr& pUndo)
{
    assert(pUndo != nullptr);

    for (const Commitment& coinToRemove : pUndo->GetCoinsAdded()) {
        m_pUpdates->SpendUTXO(coinToRemove);
    }

    std::vector<mmr::LeafIndex> leavesToAdd;
    for (const UTXO& coinToAdd : pUndo->GetCoinsSpent()) {
        leavesToAdd.push_back(coinToAdd.GetLeafIndex());
        m_pUpdates->AddUTXO(std::make_shared<UTXO>(coinToAdd));
    }

    auto pHeader = pUndo->GetPreviousHeader();
    m_pLeafSet->Rewind(pHeader->GetNumTXOs(), leavesToAdd);
    m_pKernelMMR->Rewind(pHeader->GetNumKernels());
    m_pOutputPMMR->Rewind(pHeader->GetNumTXOs());
    m_pRangeProofPMMR->Rewind(pHeader->GetNumTXOs());
    SetBestHeader(pHeader);

    // Sanity check to make sure rewind applied successfully
    ValidateMMRs(pHeader);
}

mw::Block::Ptr CoinsViewCache::BuildNextBlock(const uint64_t height, const std::vector<mw::Transaction::CPtr>& transactions)
{
    LOG_TRACE_F("Building block with {} transactions", transactions.size());
    auto pTransaction = Aggregation::Aggregate(transactions);

    std::for_each(
        pTransaction->GetKernels().cbegin(), pTransaction->GetKernels().cend(),
        [this](const Kernel& kernel) { m_pKernelMMR->Add(kernel); }
    );

    std::for_each(
        pTransaction->GetInputs().cbegin(), pTransaction->GetInputs().cend(),
        [this](const Input& input) { SpendUTXO(input.GetCommitment()); }
    );

    std::for_each(
        pTransaction->GetOutputs().cbegin(), pTransaction->GetOutputs().cend(),
        [this, height](const Output& output) { AddUTXO(height, output); }
    );

    const uint64_t output_mmr_size = m_pOutputPMMR->GetNumLeaves();
    const uint64_t kernel_mmr_size = m_pKernelMMR->GetNumLeaves();

    mw::Hash output_root = m_pOutputPMMR->Root();
    mw::Hash rangeproof_root = m_pRangeProofPMMR->Root();
    mw::Hash kernel_root = m_pKernelMMR->Root();
    mw::Hash leafset_root = m_pLeafSet->Root();

    BlindingFactor total_offset = pTransaction->GetOffset();
    if (GetBestHeader() != nullptr) {
        total_offset = Crypto::AddBlindingFactors({
            GetBestHeader()->GetOffset(),
            pTransaction->GetOffset()
        });
    }

    auto pHeader = std::make_shared<mw::Header>(
        height,
        std::move(output_root),
        std::move(rangeproof_root),
        std::move(kernel_root),
        std::move(leafset_root),
        std::move(total_offset),
        output_mmr_size,
        kernel_mmr_size
    );

    return std::make_shared<mw::Block>(pHeader, pTransaction->GetBody());
}

void CoinsViewCache::AddUTXO(const uint64_t header_height, const Output& output)
{
    mmr::LeafIndex leafIdx = m_pOutputPMMR->Add(OutputId{ output.GetFeatures(), output.GetCommitment() });
    mmr::LeafIndex leafIdx2 = m_pRangeProofPMMR->Add(*output.GetRangeProof());
    assert(leafIdx == leafIdx2);

    m_pLeafSet->Add(leafIdx);

    auto pUTXO = std::make_shared<UTXO>(header_height, std::move(leafIdx), output);

    m_pUpdates->AddUTXO(pUTXO);
}

UTXO CoinsViewCache::SpendUTXO(const Commitment& commitment)
{
    std::vector<UTXO::CPtr> utxos = GetUTXOs(commitment);
    if (utxos.empty()) {
        ThrowValidation(EConsensusError::UTXO_MISSING);
    }

    m_pLeafSet->Remove(utxos.back()->GetLeafIndex());
    m_pUpdates->SpendUTXO(commitment);

    return *utxos.back();
}

void CoinsViewCache::WriteBatch(const std::unique_ptr<libmw::IDBBatch>&, const CoinsViewUpdates& updates, const mw::Header::CPtr& pHeader)
{
    SetBestHeader(pHeader);

    for (const auto& actions : updates.GetActions()) {
        const Commitment& commitment = actions.first;
        for (const auto& action : actions.second) {
            if (action.IsSpend()) {
                m_pUpdates->SpendUTXO(commitment);
            } else {
                m_pUpdates->AddUTXO(action.pUTXO);
            }
        }
    }
}

void CoinsViewCache::Flush(const std::unique_ptr<libmw::IDBBatch>& pBatch)
{
    m_pBase->WriteBatch(pBatch, *m_pUpdates, GetBestHeader());

    m_pLeafSet->Flush();
    m_pKernelMMR->Flush();
    m_pOutputPMMR->Flush();
    m_pRangeProofPMMR->Flush();
    m_pUpdates->Clear();
}

END_NAMESPACE