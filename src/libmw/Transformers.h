#pragma once

#include <libmw/libmw.h>
#include <mw/models/tx/PegInCoin.h>
#include <mw/models/tx/PegOutCoin.h>
#include <mw/models/tx/Transaction.h>

static std::vector<PegInCoin> TransformPegIns(const std::vector<libmw::PegIn>& pegInCoins)
{
    std::vector<PegInCoin> pegins;
    std::transform(
        pegInCoins.cbegin(), pegInCoins.cend(),
        std::back_inserter(pegins),
        [](const libmw::PegIn& pegin) { return PegInCoin{ pegin.amount, Commitment{ pegin.commitment } }; }
    );

    return pegins;
}

static std::vector<PegOutCoin> TransformPegOuts(const std::vector<libmw::PegOut>& pegOutCoins)
{
    std::vector<PegOutCoin> pegouts;
    std::transform(
        pegOutCoins.cbegin(), pegOutCoins.cend(),
        std::back_inserter(pegouts),
        [](const libmw::PegOut& pegout) { return PegOutCoin{ pegout.amount, Bech32Address::FromString(pegout.address) }; }
    );

    return pegouts;
}

static std::vector<mw::Transaction::CPtr> TransformTxs(const std::vector<libmw::TxRef>& txs)
{
    std::vector<mw::Transaction::CPtr> transactions;
    std::transform(
        txs.cbegin(), txs.cend(),
        std::back_inserter(transactions),
        [](const libmw::TxRef& tx) { return tx.pTransaction; }
    );

    return transactions;
}