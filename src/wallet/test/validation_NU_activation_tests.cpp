// Copyright (c) 2021 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "wallet/test/wallet_test_fixture.h"
#include "blockassembler.h"
#include "blocksignature.h"
#include "consensus/merkle.h"
#include "primitives/transaction.h"
#include "test/librust/utiltest.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(validation_NU_activation_tests, WalletTestingSetup)

// TODO: move function to a util module (it's used inside wallet_sapling_transactions_validations_tests.cpp as well)
void setupWallet()
{
    LOCK(pwalletMain->cs_wallet);
    pwalletMain->SetMinVersion(FEATURE_SAPLING);
    gArgs.ForceSetArg("-keypool", "5");
    pwalletMain->SetupSPKM(true);
}

// TODO: move function to a util module (it's used inside wallet_sapling_transactions_validations_tests.cpp as well)
void generateAndCheckBlock(const CScript& scriptPubKey, int expectedBlockHeight, CWallet* wallet,
                           bool isPoS, std::vector<CStakeableOutput>* coins)
{
    std::unique_ptr<CBlockTemplate> pblocktemplate;
    BOOST_CHECK(pblocktemplate = BlockAssembler(Params(), false).CreateNewBlock(scriptPubKey, wallet, isPoS, coins));
    std::shared_ptr<CBlock> pblock = std::make_shared<CBlock>(pblocktemplate->block);
    pblock->nTime = WITH_LOCK(cs_main, return chainActive.Tip()->GetMedianTimePast() + 60);
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
    CValidationState state;
    BOOST_CHECK_MESSAGE(ProcessNewBlock(state, nullptr, pblock, nullptr), strprintf("Failed creating block at height %d", expectedBlockHeight));
    BOOST_CHECK(state.IsValid());
    // Let the wallet sync the blocks
    SyncWithValidationInterfaceQueue();
}

BOOST_AUTO_TEST_CASE(P2CS_new_opcode_activation_tests)
{
    SelectParams(CBaseChainParams::REGTEST);
    setupWallet();

    CTxDestination coinbaseDest;
    auto ret = pwalletMain->getNewAddress(coinbaseDest, "coinbase");
    BOOST_ASSERT_MSG(ret.result, "cannot create address");
    BOOST_ASSERT_MSG(IsValidDestination(coinbaseDest), "invalid destination");
    BOOST_ASSERT_MSG(IsMine(*pwalletMain, coinbaseDest), "destination not from wallet");

    // Create the chain
    const CScript& scriptPubKey = GetScriptForDestination(coinbaseDest);
    int nGenerateBlocks = 250;
    for (int i = 0; i < nGenerateBlocks; ++i) {
        generateAndCheckBlock(scriptPubKey, (int) i, nullptr, false, nullptr);
    }

    // Check that v6 upgrade is off
    BOOST_CHECK(!Params().GetConsensus().NetworkUpgradeActive(nGenerateBlocks, Consensus::UPGRADE_V6_0));

    // Now let's try to do a delegation using the new opcode
    CTxDestination stakerDest;
    PairResult res1 = pwalletMain->getNewStakingAddress(stakerDest, "cold_staking_address");
    BOOST_CHECK_MESSAGE(res1.result, res1.status);

    const CKeyID* stakerId = boost::get<CKeyID>(&stakerDest);
    const CKeyID* ownerId = boost::get<CKeyID>(&coinbaseDest);
    CScript csScript = GetScriptForStakeDelegation(*stakerId, *ownerId);

    CTransactionRef txRef;
    std::vector<CRecipient> vecSend;
    CAmount nAmount = 249 * COIN;
    vecSend.emplace_back(CRecipient(csScript, nAmount, false));
    CReserveKey reservekey(pwalletMain);
    CAmount nFeeRet;
    int nChangePosInOut = -1;
    std::string strFailReason;
    BOOST_CHECK(pwalletMain->CreateTransaction(vecSend, txRef, reservekey, nFeeRet, nChangePosInOut, strFailReason));

    // Validate mempool rejection.
    CValidationState mempoolState;
    BOOST_CHECK(!AcceptToMemoryPool(mempool, mempoolState, txRef, true, nullptr, false, true));
    BOOST_CHECK_EQUAL(mempoolState.GetRejectReason(), "bad-early-p2cs-v2");

    // Now try to connect the delegation ahead of time, it must fail as well.
    std::vector<CStakeableOutput> availableCoins;
    pwalletMain->StakeableCoins(&availableCoins);
    std::unique_ptr<CBlockTemplate> pblocktemplate;
    BOOST_CHECK(pblocktemplate = BlockAssembler(Params(), false).CreateNewBlock(scriptPubKey, pwalletMain, true, &availableCoins));
    std::shared_ptr<CBlock> pblock = std::make_shared<CBlock>(pblocktemplate->block);
    pblock->vtx.emplace_back(txRef);
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
    BOOST_CHECK(SignBlock(*pblock, *pwalletMain));
    CValidationState state;
    BOOST_CHECK_MESSAGE(!ProcessNewBlock(state, nullptr, pblock, nullptr), "Error: Block prior-v6 with new P2CS opcode passed");
    BOOST_CHECK(!state.IsValid());
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-early-p2cs-v2");
    BOOST_CHECK(WITH_LOCK(cs_main, return chainActive.Height()) == 250);

    // Now enforce v6 and try to connect the delegation again, it must pass
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_V6_0, 250);

    // Validate that now gets into the mempool and the wallet
    const CWallet::CommitResult& res = pwalletMain->CommitTransaction(txRef, reservekey, nullptr);
    BOOST_CHECK_EQUAL(res.status, CWallet::CommitStatus::OK);

    // Validate that now gets into a block
    CValidationState validState;
    pblock->nVersion = 10; // Must be version 10 post upgrade enforcement
    BOOST_CHECK(SignBlock(*pblock, *pwalletMain));
    BOOST_CHECK_MESSAGE(ProcessNewBlock(validState, nullptr, pblock, nullptr), "Error: Block post-v6 with new P2CS opcode failed");
    BOOST_CHECK(validState.IsValid());
    BOOST_CHECK(WITH_LOCK(cs_main, return chainActive.Height()) == 251);
    SyncWithValidationInterfaceQueue();
    CWallet::Balance balance = pwalletMain->GetBalance(1);
    BOOST_CHECK_EQUAL(balance.m_mine_cs_delegated_trusted, nAmount);
}

BOOST_AUTO_TEST_SUITE_END()
