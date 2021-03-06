#pragma once

#include <mw/models/tx/Transaction.h>
#include <mw/consensus/CutThrough.h>
#include <cassert>

static struct
{
    bool operator()(const Traits::IHashable& a, const Traits::IHashable& b) const
    {
        return a.GetHash() < b.GetHash();
    }
} SortByHash;

class Aggregation
{
public:
    //
    // Aggregates multiple transactions into 1.
    //
    static mw::Transaction::CPtr Aggregate(const std::vector<mw::Transaction::CPtr>& transactions)
    {
        if (transactions.empty()) {
            return std::make_shared<mw::Transaction>();
        }

        if (transactions.size() == 1) {
            return transactions.front();
        }

        std::vector<Input> inputs;
        std::vector<Output> outputs;
        std::vector<Kernel> kernels;
        std::vector<BlindingFactor> kernelOffsets;

        // collect all the inputs, outputs and kernels from the txs
        for (const mw::Transaction::CPtr& pTransaction : transactions) {
            for (const Input& input : pTransaction->GetInputs()) {
                inputs.push_back(input);
            }

            for (const Output& output : pTransaction->GetOutputs()) {
                outputs.push_back(output);
            }

            for (const Kernel& kernel : pTransaction->GetKernels()) {
                kernels.push_back(kernel);
            }

            kernelOffsets.push_back(pTransaction->GetOffset());
        }

        // Perform cut-through
        CutThrough::PerformCutThrough(inputs, outputs);

        // Sort the kernels.
        std::sort(kernels.begin(), kernels.end(), SortByHash);
        std::sort(inputs.begin(), inputs.end(), SortByHash);
        std::sort(outputs.begin(), outputs.end(), SortByHash);

        // Sum the kernel_offsets up to give us an aggregate offset for the transaction.
        BlindingFactor offset = Crypto::AddBlindingFactors(kernelOffsets);

        // Build a new aggregate tx from the following:
        //   * cut-through inputs
        //   * cut-through outputs
        //   * full set of tx kernels
        //   * sum of all kernel offsets
        return std::make_shared<mw::Transaction>(
            std::move(offset),
            TxBody{ std::move(inputs), std::move(outputs), std::move(kernels) }
        );
    }
};