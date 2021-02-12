// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2020 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activemasternode.h"
#include "chainparams.h"
#include "db.h"
#include "init.h"
#include "budget/budgetmanager.h"
#include "evo/deterministicmns.h"
#include "masternode-payments.h"
#include "masternode-sync.h"
#include "masternodeconfig.h"
#include "masternodeman.h"
#include "messagesigner.h"
#include "rpc/server.h"
#include "utilmoneystr.h"

#include <univalue.h>

void budgetToJSON(const CBudgetProposal* pbudgetProposal, UniValue& bObj, int nCurrentHeight)
{
    CTxDestination address1;
    ExtractDestination(pbudgetProposal->GetPayee(), address1);

    bObj.pushKV("Name", pbudgetProposal->GetName());
    bObj.pushKV("URL", pbudgetProposal->GetURL());
    bObj.pushKV("Hash", pbudgetProposal->GetHash().ToString());
    bObj.pushKV("FeeHash", pbudgetProposal->GetFeeTXHash().ToString());
    bObj.pushKV("BlockStart", (int64_t)pbudgetProposal->GetBlockStart());
    bObj.pushKV("BlockEnd", (int64_t)pbudgetProposal->GetBlockEnd());
    bObj.pushKV("TotalPaymentCount", (int64_t)pbudgetProposal->GetTotalPaymentCount());
    bObj.pushKV("RemainingPaymentCount", (int64_t)pbudgetProposal->GetRemainingPaymentCount(nCurrentHeight));
    bObj.pushKV("PaymentAddress", EncodeDestination(address1));
    bObj.pushKV("Ratio", pbudgetProposal->GetRatio());
    bObj.pushKV("Yeas", (int64_t)pbudgetProposal->GetYeas());
    bObj.pushKV("Nays", (int64_t)pbudgetProposal->GetNays());
    bObj.pushKV("Abstains", (int64_t)pbudgetProposal->GetAbstains());
    bObj.pushKV("TotalPayment", ValueFromAmount(pbudgetProposal->GetAmount() * pbudgetProposal->GetTotalPaymentCount()));
    bObj.pushKV("MonthlyPayment", ValueFromAmount(pbudgetProposal->GetAmount()));
    bObj.pushKV("IsEstablished", pbudgetProposal->IsEstablished());
    bool fValid = pbudgetProposal->IsValid();
    bObj.pushKV("IsValid", fValid);
    if (!fValid)
        bObj.pushKV("IsInvalidReason", pbudgetProposal->IsInvalidReason());
    bObj.pushKV("Allotted", ValueFromAmount(pbudgetProposal->GetAllotted()));
}

void checkBudgetInputs(const UniValue& params, std::string &strProposalName, std::string &strURL,
                       int &nPaymentCount, int &nBlockStart, CTxDestination &address, CAmount &nAmount)
{
    strProposalName = SanitizeString(params[0].get_str());
    if (strProposalName.size() > 20)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid proposal name, limit of 20 characters.");

    strURL = SanitizeString(params[1].get_str());
    std::string strErr;
    if (!validateURL(strURL, strErr))
        throw JSONRPCError(RPC_INVALID_PARAMETER, strErr);

    nPaymentCount = params[2].get_int();
    if (nPaymentCount < 1)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid payment count, must be more than zero.");

    int nMaxPayments = Params().GetConsensus().nMaxProposalPayments;
    if (nPaymentCount > nMaxPayments) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                strprintf("Invalid payment count, must be <= %d", nMaxPayments));
    }

    CBlockIndex* pindexPrev = GetChainTip();
    if (!pindexPrev)
        throw JSONRPCError(RPC_IN_WARMUP, "Try again after active chain is loaded");

    // Start must be in the next budget cycle or later
    const int budgetCycleBlocks = Params().GetConsensus().nBudgetCycleBlocks;
    int pHeight = pindexPrev->nHeight;

    int nBlockMin = pHeight - (pHeight % budgetCycleBlocks) + budgetCycleBlocks;

    nBlockStart = params[3].get_int();
    if ((nBlockStart < nBlockMin) || ((nBlockStart % budgetCycleBlocks) != 0))
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Invalid block start - must be a budget cycle block. Next valid block: %d", nBlockMin));

    address = DecodeDestination(params[4].get_str());
    if (!IsValidDestination(address))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid PIVX address");

    nAmount = AmountFromValue(params[5]);
    if (nAmount < 10 * COIN)
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Invalid amount - Payment of %s is less than minimum 10 %s allowed", FormatMoney(nAmount), CURRENCY_UNIT));

    const CAmount& nTotalBudget = g_budgetman.GetTotalBudget(nBlockStart);
    if (nAmount > nTotalBudget)
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Invalid amount - Payment of %s more than max of %s", FormatMoney(nAmount), FormatMoney(nTotalBudget)));
}

UniValue preparebudget(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 6)
        throw std::runtime_error(
            "preparebudget \"proposal-name\" \"url\" payment-count block-start \"pivx-address\" monthy-payment\n"
            "\nPrepare proposal for network by signing and creating tx\n"

            "\nArguments:\n"
            "1. \"proposal-name\":  (string, required) Desired proposal name (20 character limit)\n"
            "2. \"url\":            (string, required) URL of proposal details (64 character limit)\n"
            "3. payment-count:    (numeric, required) Total number of monthly payments\n"
            "4. block-start:      (numeric, required) Starting super block height\n"
            "5. \"pivx-address\":   (string, required) PIVX address to send payments to\n"
            "6. monthly-payment:  (numeric, required) Monthly payment amount\n"

            "\nResult:\n"
            "\"xxxx\"       (string) proposal fee hash (if successful) or error message (if failed)\n"

            "\nExamples:\n" +
            HelpExampleCli("preparebudget", "\"test-proposal\" \"https://forum.pivx.org/t/test-proposal\" 2 820800 \"D9oc6C3dttUbv8zd7zGNq1qKBGf4ZQ1XEE\" 500") +
            HelpExampleRpc("preparebudget", "\"test-proposal\" \"https://forum.pivx.org/t/test-proposal\" 2 820800 \"D9oc6C3dttUbv8zd7zGNq1qKBGf4ZQ1XEE\" 500"));

    if (!pwalletMain) {
        throw JSONRPCError(RPC_IN_WARMUP, "Try again after active chain is loaded");
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    std::string strProposalName;
    std::string strURL;
    int nPaymentCount;
    int nBlockStart;
    CTxDestination address;
    CAmount nAmount;

    checkBudgetInputs(request.params, strProposalName, strURL, nPaymentCount, nBlockStart, address, nAmount);

    // Parse PIVX address
    CScript scriptPubKey = GetScriptForDestination(address);

    // create transaction 15 minutes into the future, to allow for confirmation time
    CBudgetProposal proposal(strProposalName, strURL, nPaymentCount, scriptPubKey, nAmount, nBlockStart, UINT256_ZERO);
    const uint256& nHash = proposal.GetHash();
    if (!proposal.IsWellFormed(g_budgetman.GetTotalBudget(proposal.GetBlockStart())))
        throw std::runtime_error("Proposal is not valid " + proposal.IsInvalidReason());

    CTransactionRef wtx;
    // make our change address
    CReserveKey keyChange(pwalletMain);
    if (!pwalletMain->CreateBudgetFeeTX(wtx, nHash, keyChange, false)) { // 50 PIV collateral for proposal
        throw std::runtime_error("Error making collateral transaction for proposal. Please check your wallet balance.");
    }

    //send the tx to the network
    const CWallet::CommitResult& res = pwalletMain->CommitTransaction(wtx, keyChange, g_connman.get());
    if (res.status != CWallet::CommitStatus::OK)
        throw JSONRPCError(RPC_WALLET_ERROR, res.ToString());

    // Store proposal name as a comment
    assert(pwalletMain->mapWallet.count(wtx->GetHash()));
    pwalletMain->mapWallet.at(wtx->GetHash()).SetComment("Proposal: " + strProposalName);

    return wtx->GetHash().ToString();
}

UniValue submitbudget(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 7)
        throw std::runtime_error(
            "submitbudget \"proposal-name\" \"url\" payment-count block-start \"pivx-address\" monthly-payment \"fee-tx\"\n"
            "\nSubmit proposal to the network\n"

            "\nArguments:\n"
            "1. \"proposal-name\":  (string, required) Desired proposal name (20 character limit)\n"
            "2. \"url\":            (string, required) URL of proposal details (64 character limit)\n"
            "3. payment-count:    (numeric, required) Total number of monthly payments\n"
            "4. block-start:      (numeric, required) Starting super block height\n"
            "5. \"pivx-address\":   (string, required) PIVX address to send payments to\n"
            "6. monthly-payment:  (numeric, required) Monthly payment amount\n"
            "7. \"fee-tx\":         (string, required) Transaction hash from preparebudget command\n"

            "\nResult:\n"
            "\"xxxx\"       (string) proposal hash (if successful) or error message (if failed)\n"

            "\nExamples:\n" +
            HelpExampleCli("submitbudget", "\"test-proposal\" \"https://forum.pivx.org/t/test-proposal\" 2 820800 \"D9oc6C3dttUbv8zd7zGNq1qKBGf4ZQ1XEE\" 500") +
            HelpExampleRpc("submitbudget", "\"test-proposal\" \"https://forum.pivx.org/t/test-proposal\" 2 820800 \"D9oc6C3dttUbv8zd7zGNq1qKBGf4ZQ1XEE\" 500"));

    std::string strProposalName;
    std::string strURL;
    int nPaymentCount;
    int nBlockStart;
    CTxDestination address;
    CAmount nAmount;

    checkBudgetInputs(request.params, strProposalName, strURL, nPaymentCount, nBlockStart, address, nAmount);

    // Parse PIVX address
    CScript scriptPubKey = GetScriptForDestination(address);
    const uint256& hash = ParseHashV(request.params[6], "parameter 1");

    if (!masternodeSync.IsBlockchainSynced()) {
        throw std::runtime_error("Must wait for client to sync with masternode network. Try again in a minute or so.");
    }

    // create the proposal in case we're the first to make it
    CBudgetProposal proposal(strProposalName, strURL, nPaymentCount, scriptPubKey, nAmount, nBlockStart, hash);
    if(!g_budgetman.AddProposal(proposal)) {
        std::string strError = strprintf("invalid budget proposal - %s", proposal.IsInvalidReason());
        throw std::runtime_error(strError);
    }
    proposal.Relay();

    return proposal.GetHash().ToString();
}

static UniValue packRetStatus(const std::string& nodeType, const std::string& result, const std::string& error)
{
    UniValue statusObj(UniValue::VOBJ);
    statusObj.pushKV("node", nodeType);
    statusObj.pushKV("result", result);
    statusObj.pushKV("error", error);
    return statusObj;
}

static UniValue packErrorRetStatus(const std::string& nodeType, const std::string& error)
{
    return packRetStatus(nodeType, "failed", error);
}

static bool voteProposal(const COutPoint& collOut, const CKey& keyMasternode, const std::string& mnAlias,
                         const uint256& propHash, const CBudgetVote::VoteDirection& nVote,
                         UniValue& resultsObj)
{
    CBudgetVote vote(CTxIn(collOut), propHash, nVote);
    if (!vote.Sign(keyMasternode, keyMasternode.GetPubKey().GetID())) {
        resultsObj.push_back(packErrorRetStatus(mnAlias, "Failure to sign."));
        return false;
    }

    std::string strError;
    if (!g_budgetman.AddAndRelayProposalVote(vote, strError)) {
        resultsObj.push_back(packErrorRetStatus(mnAlias, strError));
        return false;
    }

    resultsObj.push_back(packRetStatus(mnAlias, "success", ""));
    return true;
}

static bool voteFinalBudget(const COutPoint& collOut, const CKey& votingKey, const std::string& mnAlias,
                            const uint256& budgetHash, UniValue& resultsObj)
{
    CFinalizedBudgetVote vote(CTxIn(collOut), budgetHash);
    if (!vote.Sign(votingKey, votingKey.GetPubKey().GetID())) {
        resultsObj.push_back(packErrorRetStatus(mnAlias, "Failure to sign."));
        return false;
    }

    std::string strError = "";
    if (!g_budgetman.UpdateFinalizedBudget(vote, nullptr, strError)) {
        resultsObj.push_back(packErrorRetStatus(mnAlias, strError));
        return false;
    }

    g_budgetman.AddSeenFinalizedBudgetVote(vote);
    vote.Relay();
    resultsObj.push_back(packRetStatus(mnAlias, "success", ""));
    return true;
}

// Legacy masternodes
static bool getMNKeysForEntry(const CMasternodeConfig::CMasternodeEntry& mne,
                              CKey& key, CPubKey& pubkey, COutPoint& collOut, UniValue& resultsObj)
{
    if (!CMessageSigner::GetKeysFromSecret(mne.getPrivKey(), key, pubkey)) {
        resultsObj.push_back(
                packErrorRetStatus(mne.getAlias(), "Masternode signing error, could not set key correctly."));
        return false;
    }
    CMasternode* pmn = mnodeman.Find(pubkey);
    if (!pmn) {
        resultsObj.push_back(packErrorRetStatus(mne.getAlias(), "Can't find masternode by pubkey"));
        return false;
    }
    collOut = pmn->vin.prevout;
    return true;
}

static bool getMNKeysForActiveMasternode(CKey& key, CPubKey& pubkey, COutPoint& collOut, UniValue& resultsObj)
{
    // local node must be a masternode
    if (!fMasterNode)
        throw JSONRPCError(RPC_MISC_ERROR, _("This is not a masternode. 'local' option disabled."));

    if (activeMasternode.vin == nullopt)
        throw JSONRPCError(RPC_MISC_ERROR, _("Active Masternode not initialized."));

    activeMasternode.GetKeys(key, pubkey);
    CMasternode* pmn = mnodeman.Find(pubkey);
    if (!pmn) {
        resultsObj.push_back(packErrorRetStatus("local", "Can't find masternode by pubkey"));
        return false;
    }
    collOut = pmn->vin.prevout;
    return true;
}

// Deterministic masternodes
static void voteProposalWithDeterministicMNs(const std::map<std::pair<uint256, COutPoint>, CKey>& votingKeys,
                                             const uint256& propHash, const CBudgetVote::VoteDirection& nVote,
                                             UniValue& resultsObj, int& success, int& failed)
{
    for (const auto& it : votingKeys) {
        const uint256& proTxHash = it.first.first;
        const COutPoint& collatOut = it.first.second;
        const CKey& votingKey = it.second;
        if (voteProposal(collatOut, votingKey, proTxHash.ToString(), propHash, nVote, resultsObj)) {
            success++;
        } else {
            failed++;
        }
    }
}

static UniValue packVoteReturnValue(const UniValue& details, int success, int failed)
{
    UniValue returnObj(UniValue::VOBJ);
    returnObj.pushKV("overall", strprintf("Voted successfully %d time(s) and failed %d time(s).", success, failed));
    returnObj.pushKV("detail", details);
    return returnObj;
}

// vote on proposal (finalized budget, if final=true) with all possible keys or a single mn (mnAliasFilter)
static UniValue mnBudgetVoteInner(bool fLegacyMN, Optional<std::string> mnAliasFilter, const uint256& budgetHash,
                                  const CBudgetVote::VoteDirection& nVote, bool final)
{
    UniValue resultsObj(UniValue::VARR);
    int success = 0;
    int failed = 0;

    if (!fLegacyMN) {
        // !TODO: add voting for finalized budget
        // Deterministic masternode voting. Need wallet with voting key.
        if (!pwalletMain) {
            throw JSONRPCError(RPC_IN_WARMUP, "Wallet (with voting key) not found.");
        }

        LOCK(pwalletMain->cs_wallet);
        EnsureWalletIsUnlocked();

        // <proTx, collatOut> --> votingKey map
        std::map<std::pair<uint256, COutPoint>, CKey> votingKeys;
        auto mnList = deterministicMNManager->GetListAtChainTip();

        if (mnAliasFilter) {
            // vote with a single masternode (identified by ProTx)
            const uint256& proTxHash = ParseHashV(*mnAliasFilter, "ProTX transaction hash");
            auto dmn = mnList.GetValidMN(proTxHash);
            if (!dmn) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid or unknown proTxHash");
            }
            CKey votingKey;
            if (!pwalletMain->GetKey(dmn->pdmnState->keyIDVoting, votingKey)) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Private key for voting address %s not known by wallet", EncodeDestination(dmn->pdmnState->keyIDVoting)));
            }
            votingKeys.emplace(std::piecewise_construct,
                               std::forward_as_tuple(proTxHash, dmn->collateralOutpoint),
                               std::forward_as_tuple(votingKey));
        } else {
            // vote with all voting keys known by this wallet
            mnList.ForEachMN(true, [&](const CDeterministicMNCPtr& dmn) {
                CKey votingKey;
                if (pwalletMain->GetKey(dmn->pdmnState->keyIDVoting, votingKey)) {
                    votingKeys.emplace(std::piecewise_construct,
                                       std::forward_as_tuple(dmn->proTxHash, dmn->collateralOutpoint),
                                       std::forward_as_tuple(votingKey));
                }
            });
        }

        voteProposalWithDeterministicMNs(votingKeys, budgetHash, nVote, resultsObj, success, failed);
        return packVoteReturnValue(resultsObj, success, failed);
    }

    // Legacy Masternodes
    for (const CMasternodeConfig::CMasternodeEntry& mne : masternodeConfig.getEntries()) {
        if (mnAliasFilter && *mnAliasFilter != mne.getAlias()) continue;
        CKey keyMasternode; CPubKey pubKeyMasternode; COutPoint collOut;
        bool result = getMNKeysForEntry(mne, keyMasternode, pubKeyMasternode, collOut, resultsObj) &&
                      (final ? voteFinalBudget(collOut, keyMasternode, mne.getAlias(), budgetHash, resultsObj)
                             : voteProposal(collOut, keyMasternode, mne.getAlias(), budgetHash, nVote, resultsObj));
        if (result) success++;
        else failed++;
    }
    return packVoteReturnValue(resultsObj, success, failed);
}

// vote on proposal (finalized budget, if final=true) with the active local masternode
// Note: for DMNs only finalized budget voting is allowed with the operator key
// (proposal voting requires the voting key)
static UniValue mnLocalBudgetVoteInner(const uint256& budgetHash, const CBudgetVote::VoteDirection& nVote, bool final)
{
    UniValue resultsObj(UniValue::VARR);
    CKey keyMasternode; CPubKey pubKeyMasternode; COutPoint collOut;
    bool ret = getMNKeysForActiveMasternode(keyMasternode, pubKeyMasternode, collOut, resultsObj) &&
               (final ? voteFinalBudget(collOut, keyMasternode, "local", budgetHash, resultsObj)
                      : voteProposal(collOut, keyMasternode, "local", budgetHash, nVote, resultsObj));
    return packVoteReturnValue(resultsObj, ret, !ret);
}

static CBudgetVote::VoteDirection parseVote(const std::string& strVote)
{
    if (strVote != "yes" && strVote != "no") throw JSONRPCError(RPC_MISC_ERROR, "You can only vote 'yes' or 'no'");
    CBudgetVote::VoteDirection nVote = CBudgetVote::VOTE_ABSTAIN;
    if (strVote == "yes") nVote = CBudgetVote::VOTE_YES;
    if (strVote == "no") nVote = CBudgetVote::VOTE_NO;
    return nVote;
}

UniValue mnbudgetvote(const JSONRPCRequest& request)
{
    std::string strCommand;
    if (request.params.size() >= 1) {
        strCommand = request.params[0].get_str();

        // Backwards compatibility with legacy `mnbudget` command
        if (strCommand == "vote") strCommand = "local";
        if (strCommand == "vote-many") strCommand = "many";
        if (strCommand == "vote-alias") strCommand = "alias";
    }

    if (request.fHelp || (request.params.size() == 3 && (strCommand != "local" && strCommand != "many")) || (request.params.size() == 4 && strCommand != "alias") ||
        request.params.size() > 5 || request.params.size() < 3)
        throw std::runtime_error(
            "mnbudgetvote \"local|many|alias\" \"votehash\" \"yes|no\" ( \"alias\" legacy )\n"
            "\nVote on a budget proposal\n"
            "\nAfter V6 enforcement, the deterministic masternode system is used by default. Set the \"legacy\" parameter to true to vote with legacy masternodes."

            "\nArguments:\n"
            "1. \"mode\"      (string, required) The voting mode. 'local' for voting directly from a masternode, 'many' for voting with a MN controller and casting the same vote for each MN, 'alias' for voting with a MN controller and casting a vote for a single MN\n"
            "2. \"votehash\"  (string, required) The vote hash for the proposal\n"
            "3. \"votecast\"  (string, required) Your vote. 'yes' to vote for the proposal, 'no' to vote against\n"
            "4. \"alias\"     (string, required for 'alias' mode) The MN alias to cast a vote for (for deterministic masternodes it's the hash of the proTx transaction).\n"
            "5. \"legacy\"    (boolean, optional, default=false) Use the legacy masternode system after deterministic masternodes enforcement.\n"

            "\nResult:\n"
            "{\n"
            "  \"overall\": \"xxxx\",      (string) The overall status message for the vote cast\n"
            "  \"detail\": [\n"
            "    {\n"
            "      \"node\": \"xxxx\",      (string) 'local' or the MN alias\n"
            "      \"result\": \"xxxx\",    (string) Either 'Success' or 'Failed'\n"
            "      \"error\": \"xxxx\",     (string) Error message, if vote failed\n"
            "    }\n"
            "    ,...\n"
            "  ]\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("mnbudgetvote", "\"alias\" \"ed2f83cedee59a91406f5f47ec4d60bf5a7f9ee6293913c82976bd2d3a658041\" \"yes\" \"4f9de28fca1f0574a217c5d3c59cc51125ec671de82a2f80b6ceb69673115041\"") +
            HelpExampleRpc("mnbudgetvote", "\"alias\" \"ed2f83cedee59a91406f5f47ec4d60bf5a7f9ee6293913c82976bd2d3a658041\" \"yes\" \"4f9de28fca1f0574a217c5d3c59cc51125ec671de82a2f80b6ceb69673115041\""));

    const uint256& hash = ParseHashV(request.params[1], "parameter 1");
    CBudgetVote::VoteDirection nVote = parseVote(request.params[2].get_str());

    bool fLegacyMN = !deterministicMNManager->IsDIP3Enforced() || (request.params.size() > 4 && request.params[4].get_bool());

    if (strCommand == "local") {
        if (!fLegacyMN) {
            throw JSONRPCError(RPC_MISC_ERROR, _("\"local\" vote is no longer available with DMNs. Use \"alias\" from the wallet with the voting key."));
        }
        return mnLocalBudgetVoteInner(hash, nVote, false);
    }

    bool isAlias = false;
    if (strCommand == "many" || (isAlias = strCommand == "alias")) {
        Optional<std::string> mnAlias = isAlias ? Optional<std::string>(request.params[3].get_str()) : nullopt;
        return mnBudgetVoteInner(fLegacyMN, mnAlias, hash, nVote, false);
    }

    return NullUniValue;
}

UniValue getbudgetvotes(const JSONRPCRequest& request)
{
    if (request.params.size() != 1)
        throw std::runtime_error(
            "getbudgetvotes \"proposal-name\"\n"
            "\nPrint vote information for a budget proposal\n"

            "\nArguments:\n"
            "1. \"proposal-name\":      (string, required) Name of the proposal\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"mnId\": \"xxxx-x\",      (string) Masternode's outpoint collateral transaction (hash-n)\n"
            "    \"nHash\": \"xxxx\",       (string) Hash of the vote\n"
            "    \"Vote\": \"YES|NO\",      (string) Vote cast ('YES' or 'NO')\n"
            "    \"nTime\": xxxx,         (numeric) Time in seconds since epoch the vote was cast\n"
            "    \"fValid\": true|false,  (boolean) 'true' if the vote is valid, 'false' otherwise\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("getbudgetvotes", "\"test-proposal\"") + HelpExampleRpc("getbudgetvotes", "\"test-proposal\""));

    std::string strProposalName = SanitizeString(request.params[0].get_str());
    const CBudgetProposal* pbudgetProposal = g_budgetman.FindProposalByName(strProposalName);
    if (pbudgetProposal == NULL) throw std::runtime_error("Unknown proposal name");
    return pbudgetProposal->GetVotesArray();
}

UniValue getnextsuperblock(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "getnextsuperblock\n"
            "\nPrint the next super block height\n"

            "\nResult:\n"
            "n      (numeric) Block height of the next super block\n"

            "\nExamples:\n" +
            HelpExampleCli("getnextsuperblock", "") + HelpExampleRpc("getnextsuperblock", ""));

    int nChainHeight = WITH_LOCK(cs_main, return chainActive.Height());
    if (nChainHeight < 0) return "unknown";

    const int nBlocksPerCycle = Params().GetConsensus().nBudgetCycleBlocks;
    int nNext = nChainHeight - nChainHeight % nBlocksPerCycle + nBlocksPerCycle;
    return nNext;
}

UniValue getbudgetprojection(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "getbudgetprojection\n"
            "\nShow the projection of which proposals will be paid the next cycle\n"
            "Proposal fee tx time need to be +24hrs old from the current time. (Testnet is 5 mins)\n"
            "Net Votes needs to be above Masternode Count divided by 10\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"Name\": \"xxxx\",               (string) Proposal Name\n"
            "    \"URL\": \"xxxx\",                (string) Proposal URL\n"
            "    \"Hash\": \"xxxx\",               (string) Proposal vote hash\n"
            "    \"FeeHash\": \"xxxx\",            (string) Proposal fee hash\n"
            "    \"BlockStart\": n,              (numeric) Proposal starting block\n"
            "    \"BlockEnd\": n,                (numeric) Proposal ending block\n"
            "    \"TotalPaymentCount\": n,       (numeric) Number of payments\n"
            "    \"RemainingPaymentCount\": n,   (numeric) Number of remaining payments\n"
            "    \"PaymentAddress\": \"xxxx\",     (string) PIVX address of payment\n"
            "    \"Ratio\": x.xxx,               (numeric) Ratio of yeas vs nays\n"
            "    \"Yeas\": n,                    (numeric) Number of yea votes\n"
            "    \"Nays\": n,                    (numeric) Number of nay votes\n"
            "    \"Abstains\": n,                (numeric) Number of abstains\n"
            "    \"TotalPayment\": xxx.xxx,      (numeric) Total payment amount in PIV\n"
            "    \"MonthlyPayment\": xxx.xxx,    (numeric) Monthly payment amount in PIV\n"
            "    \"IsEstablished\": true|false,  (boolean) Proposal is considered established, 24 hrs after being submitted to network. (Testnet is 5 mins)\n"
            "    \"IsValid\": true|false,        (boolean) Valid (true) or Invalid (false)\n"
            "    \"IsInvalidReason\": \"xxxx\",  (string) Error message, if any\n"
            "    \"Allotted\": xxx.xxx,           (numeric) Amount of PIV allotted in current period\n"
            "    \"TotalBudgetAllotted\": xxx.xxx (numeric) Total PIV allotted\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("getbudgetprojection", "") + HelpExampleRpc("getbudgetprojection", ""));

    UniValue ret(UniValue::VARR);
    UniValue resultObj(UniValue::VOBJ);
    CAmount nTotalAllotted = 0;

    std::vector<CBudgetProposal> winningProps = g_budgetman.GetBudget();
    for (const CBudgetProposal& p : winningProps) {
        UniValue bObj(UniValue::VOBJ);
        budgetToJSON(&p, bObj, g_budgetman.GetBestHeight());
        nTotalAllotted += p.GetAllotted();
        bObj.pushKV("TotalBudgetAllotted", ValueFromAmount(nTotalAllotted));
        ret.push_back(bObj);
    }

    return ret;
}

UniValue getbudgetinfo(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            "getbudgetinfo ( \"proposal\" )\n"
            "\nShow current masternode budgets\n"

            "\nArguments:\n"
            "1. \"proposal\"    (string, optional) Proposal name\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"Name\": \"xxxx\",               (string) Proposal Name\n"
            "    \"URL\": \"xxxx\",                (string) Proposal URL\n"
            "    \"Hash\": \"xxxx\",               (string) Proposal vote hash\n"
            "    \"FeeHash\": \"xxxx\",            (string) Proposal fee hash\n"
            "    \"BlockStart\": n,              (numeric) Proposal starting block\n"
            "    \"BlockEnd\": n,                (numeric) Proposal ending block\n"
            "    \"TotalPaymentCount\": n,       (numeric) Number of payments\n"
            "    \"RemainingPaymentCount\": n,   (numeric) Number of remaining payments\n"
            "    \"PaymentAddress\": \"xxxx\",     (string) PIVX address of payment\n"
            "    \"Ratio\": x.xxx,               (numeric) Ratio of yeas vs nays\n"
            "    \"Yeas\": n,                    (numeric) Number of yea votes\n"
            "    \"Nays\": n,                    (numeric) Number of nay votes\n"
            "    \"Abstains\": n,                (numeric) Number of abstains\n"
            "    \"TotalPayment\": xxx.xxx,      (numeric) Total payment amount in PIV\n"
            "    \"MonthlyPayment\": xxx.xxx,    (numeric) Monthly payment amount in PIV\n"
            "    \"IsEstablished\": true|false,  (boolean) Proposal is considered established, 24 hrs after being submitted to network. (5 mins for Testnet)\n"
            "    \"IsValid\": true|false,        (boolean) Valid (true) or Invalid (false)\n"
            "    \"IsInvalidReason\": \"xxxx\",      (string) Error message, if any\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("getbudgetprojection", "") + HelpExampleRpc("getbudgetprojection", ""));

    UniValue ret(UniValue::VARR);
    int nCurrentHeight = g_budgetman.GetBestHeight();

    if (request.params.size() == 1) {
        std::string strProposalName = SanitizeString(request.params[0].get_str());
        const CBudgetProposal* pbudgetProposal = g_budgetman.FindProposalByName(strProposalName);
        if (pbudgetProposal == NULL) throw std::runtime_error("Unknown proposal name");
        UniValue bObj(UniValue::VOBJ);
        budgetToJSON(pbudgetProposal, bObj, nCurrentHeight);
        ret.push_back(bObj);
        return ret;
    }

    std::vector<CBudgetProposal*> winningProps = g_budgetman.GetAllProposals();
    for (CBudgetProposal* pbudgetProposal : winningProps) {
        if (!pbudgetProposal->IsValid()) continue;

        UniValue bObj(UniValue::VOBJ);
        budgetToJSON(pbudgetProposal, bObj, nCurrentHeight);
        ret.push_back(bObj);
    }

    return ret;
}

UniValue mnbudgetrawvote(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 6)
        throw std::runtime_error(
            "mnbudgetrawvote \"masternode-tx-hash\" masternode-tx-index \"proposal-hash\" yes|no time \"vote-sig\"\n"
            "\nCompile and relay a proposal vote with provided external signature instead of signing vote internally\n"

            "\nArguments:\n"
            "1. \"masternode-tx-hash\"  (string, required) Transaction hash for the masternode\n"
            "2. masternode-tx-index   (numeric, required) Output index for the masternode\n"
            "3. \"proposal-hash\"       (string, required) Proposal vote hash\n"
            "4. yes|no                (boolean, required) Vote to cast\n"
            "5. time                  (numeric, required) Time since epoch in seconds\n"
            "6. \"vote-sig\"            (string, required) External signature\n"

            "\nResult:\n"
            "\"status\"     (string) Vote status or error message\n"

            "\nExamples:\n" +
            HelpExampleCli("mnbudgetrawvote", "") + HelpExampleRpc("mnbudgetrawvote", ""));

    const uint256& hashMnTx = ParseHashV(request.params[0], "mn tx hash");
    int nMnTxIndex = request.params[1].get_int();
    const CTxIn& vin = CTxIn(hashMnTx, nMnTxIndex);

    const uint256& hashProposal = ParseHashV(request.params[2], "Proposal hash");
    CBudgetVote::VoteDirection nVote = parseVote(request.params[3].get_str());

    int64_t nTime = request.params[4].get_int64();
    std::string strSig = request.params[5].get_str();
    bool fInvalid = false;
    std::vector<unsigned char> vchSig = DecodeBase64(strSig.c_str(), &fInvalid);

    if (fInvalid)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Malformed base64 encoding");

    CMasternode* pmn = mnodeman.Find(vin.prevout);
    if (!pmn) {
        return "Failure to find masternode in list : " + vin.ToString();
    }

    CBudgetVote vote(vin, hashProposal, nVote);
    vote.SetTime(nTime);
    vote.SetVchSig(vchSig);

    if (!vote.CheckSignature(pmn->pubKeyMasternode.GetID())) {
        // try old message version
        vote.nMessVersion = MessageVersion::MESS_VER_STRMESS;
        if (!vote.CheckSignature(pmn->pubKeyMasternode.GetID())) return "Failure to verify signature.";
    }

    std::string strError;
    if (g_budgetman.AddAndRelayProposalVote(vote, strError)) {
        return "Voted successfully";
    } else {
        return "Error voting : " + strError;
    }
}

UniValue mnfinalbudgetsuggest(const JSONRPCRequest& request)
{
    if (request.fHelp || !request.params.empty())
        throw std::runtime_error(
                "mnfinalbudgetsuggest\n"
                "\nTry to submit a budget finalization\n"
                "returns the budget hash if it was broadcasted sucessfully");

    if (!Params().IsRegTestNet()) {
        throw JSONRPCError(RPC_MISC_ERROR, "command available only for RegTest network");
    }

    const uint256& budgetHash = g_budgetman.SubmitFinalBudget();
    return (budgetHash.IsNull()) ? NullUniValue : budgetHash.ToString();
}

UniValue mnfinalbudget(const JSONRPCRequest& request)
{
    std::string strCommand;
    if (request.params.size() >= 1)
        strCommand = request.params[0].get_str();

    if (request.fHelp ||
        (strCommand != "vote-many" && strCommand != "vote" && strCommand != "show" && strCommand != "getvotes"))
        throw std::runtime_error(
            "mnfinalbudget \"command\"... ( \"passphrase\" )\n"
            "\nVote or show current budgets\n"

            "\nAvailable commands:\n"
            "  vote-many   - Vote on a finalized budget\n"
            "  vote        - Vote on a finalized budget with local masternode\n"
            "  show        - Show existing finalized budgets\n"
            "  getvotes     - Get vote information for each finalized budget\n");

    if (strCommand == "vote-many" || strCommand == "vote") {
        if (request.params.size() != 2)
            throw std::runtime_error(strprintf("Correct usage is 'mnfinalbudget %s BUDGET_HASH'", strCommand));

        const uint256& hash = ParseHashV(request.params[1], "BUDGET_HASH");
        return (strCommand == "vote-many" ? mnBudgetVoteInner(true, nullopt, hash, CBudgetVote::VOTE_YES, true)
                                          : mnLocalBudgetVoteInner(hash, CBudgetVote::VOTE_YES, true));
    }

    if (strCommand == "show") {
        UniValue resultObj(UniValue::VOBJ);

        std::vector<CFinalizedBudget*> winningFbs = g_budgetman.GetFinalizedBudgets();
        for (CFinalizedBudget* finalizedBudget : winningFbs) {
            const uint256& nHash = finalizedBudget->GetHash();
            UniValue bObj(UniValue::VOBJ);
            bObj.pushKV("FeeTX", finalizedBudget->GetFeeTXHash().ToString());
            bObj.pushKV("BlockStart", (int64_t)finalizedBudget->GetBlockStart());
            bObj.pushKV("BlockEnd", (int64_t)finalizedBudget->GetBlockEnd());
            bObj.pushKV("Proposals", finalizedBudget->GetProposalsStr());
            bObj.pushKV("VoteCount", (int64_t)finalizedBudget->GetVoteCount());
            bObj.pushKV("Status", g_budgetman.GetFinalizedBudgetStatus(nHash));

            bool fValid = finalizedBudget->IsValid();
            bObj.pushKV("IsValid", fValid);
            if (!fValid)
                bObj.pushKV("IsInvalidReason", finalizedBudget->IsInvalidReason());

            std::string strName = strprintf("%s (%s)", finalizedBudget->GetName(), nHash.ToString());
            resultObj.pushKV(strName, bObj);
        }

        return resultObj;
    }

    if (strCommand == "getvotes") {
        if (request.params.size() != 2)
            throw std::runtime_error("Correct usage is 'mnbudget getvotes budget-hash'");

        LOCK(g_budgetman.cs_budgets);
        std::string strHash = request.params[1].get_str();
        uint256 hash(uint256S(strHash));
        CFinalizedBudget* pfinalBudget = g_budgetman.FindFinalizedBudget(hash);
        if (pfinalBudget == NULL) return "Unknown budget hash";
        return pfinalBudget->GetVotesObject();
    }

    return NullUniValue;
}

UniValue checkbudgets(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "checkbudgets\n"
            "\nInitiates a budget check cycle manually\n"

            "\nExamples:\n" +
            HelpExampleCli("checkbudgets", "") + HelpExampleRpc("checkbudgets", ""));

    if (!masternodeSync.IsSynced())
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "Masternode/Budget sync not finished yet");

    g_budgetman.CheckAndRemove();
    return NullUniValue;
}

static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         okSafeMode
  //  --------------------- ------------------------  -----------------------  ----------
    { "budget",             "preparebudget",          &preparebudget,          true  },
    { "budget",             "submitbudget",           &submitbudget,           true  },
    { "budget",             "mnbudgetvote",           &mnbudgetvote,           true  },
    { "budget",             "getbudgetvotes",         &getbudgetvotes,         true  },
    { "budget",             "getnextsuperblock",      &getnextsuperblock,      true  },
    { "budget",             "getbudgetprojection",    &getbudgetprojection,    true  },
    { "budget",             "getbudgetinfo",          &getbudgetinfo,          true  },
    { "budget",             "mnbudgetrawvote",        &mnbudgetrawvote,        true  },
    { "budget",             "mnfinalbudget",          &mnfinalbudget,          true  },
    { "budget",             "checkbudgets",           &checkbudgets,           true  },

    /* Not shown in help */
    { "hidden",             "mnfinalbudgetsuggest",   &mnfinalbudgetsuggest,   true  },

};

void RegisterBudgetRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
