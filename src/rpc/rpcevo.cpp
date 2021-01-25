// Copyright (c) 2018-2021 The Dash Core developers
// Copyright (c) 2021 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "core_io.h"
#include "destination_io.h"
#include "evo/specialtx.h"
#include "evo/providertx.h"
#include "masternode.h"
#include "messagesigner.h"
#include "netbase.h"
#include "pubkey.h" // COMPACT_SIGNATURE_SIZE
#include "rpc/server.h"
#include "utilmoneystr.h"
#include "validation.h"

#ifdef ENABLE_WALLET
#include "coincontrol.h"
#include "wallet/wallet.h"
#include "wallet/rpcwallet.h"

extern UniValue signrawtransaction(const JSONRPCRequest& request);
extern UniValue sendrawtransaction(const JSONRPCRequest& request);
#endif//ENABLE_WALLET

enum ProRegParam {
    collateralAddress,
    collateralHash,
    collateralIndex,
    ipAndPort_register,
    ipAndPort_update,
    operatorAddress_register,
    operatorAddress_update,
    operatorPayoutAddress_register,
    operatorPayoutAddress_update,
    operatorReward,
    operatorKey,
    ownerAddress,
    ownerKey,
    proTxHash,
    payoutAddress_register,
    payoutAddress_update,
    votingAddress_register,
    votingAddress_update,
};

static const std::map<ProRegParam, std::string> mapParamHelp = {
        {collateralAddress,
            "%d. \"collateralAddress\"     (string, required) The PIVX address to send the collateral to.\n"
        },
        {collateralHash,
            "%d. \"collateralHash\"        (string, required) The collateral transaction hash.\n"
        },
        {collateralIndex,
            "%d. collateralIndex           (numeric, required) The collateral transaction output index.\n"
        },
        {ipAndPort_register,
            "%d. \"ipAndPort\"             (string, required) IP and port in the form \"IP:PORT\".\n"
            "                                Must be unique on the network. Can be set to 0, which will require a ProUpServTx afterwards.\n"
        },
        {ipAndPort_update,
            "%d. \"ipAndPort\"             (string, required) IP and port in the form \"IP:PORT\".\n"
            "                                If set to an empty string, the currently active ip is reused.\n"
        },
        {operatorAddress_register,
            "%d. \"operatorAddress\"       (string, required) The operator key address. The private key does not have to be known by your wallet.\n"
            "                                If set to an empty string, ownerAddr will be used.\n"
        },
        {operatorAddress_update,
            "%d. \"operatorAddress\"       (string, required) The operator key address. The private key does not have to be known by your wallet.\n"
            "                                 It has to match the private key which can be used later to update the service.\n"
            "                                 If set to an empty string, the currently active operator key is reused.\n"
        },
        {operatorKey,
            "%d. \"operatorKey\"           (string, optional) The operator key associated with the operator address of the masternode.\n"
            "                                If not specified, or set to an empty string, then the mn key must be known by your wallet, in order to sign the tx.\n"
        },
        {operatorPayoutAddress_register,
            "%d. \"operatorPayoutAddress\" (string, optional) The address used for operator reward payments.\n"
            "                                Only allowed when the ProRegTx had a non-zero operatorReward value.\n"
            "                                If set to an empty string, the operatorAddress is used.\n"
        },
        {operatorPayoutAddress_update,
            "%d. \"operatorPayoutAddress\" (string, optional) The address used for operator reward payments.\n"
            "                                Only allowed when the ProRegTx had a non-zero operatorReward value.\n"
            "                                If set to an empty string, the currently active one is reused.\n"
        },
        {operatorReward,
            "%d. \"operatorReward\"        (numeric, optional) The fraction in %% to share with the operator. The value must be\n"
            "                                between 0.00 and 100.00. If not set, it takes the default value of 0.0\n"
        },
        {ownerAddress,
            "%d. \"ownerAddress\"          (string, required) The PIVX address to use for payee updates and proposal voting.\n"
            "                                The private key belonging to this address must be known in your wallet, in order to send updates.\n"
            "                                The address must not be already registered, and must differ from the collateralAddress\n"
        },
        {ownerKey,
            "%d. \"ownerKey\"              (string, optional) The owner key associated with the operator address of the masternode.\n"
            "                                If not specified, or set to an empty string, then the mn key must be known by your wallet, in order to sign the tx.\n"
        },
        {payoutAddress_register,
            "%d. \"payoutAddress\"          (string, required) The PIVX address to use for masternode reward payments.\n"
        },
        {payoutAddress_update,
            "%d. \"payoutAddress\"          (string, required) The PIVX address to use for masternode reward payments.\n"
            "                                 If set to an empty string, the currently active payout address is reused.\n"
        },
        {proTxHash,
            "%d. \"proTxHash\"              (string, required) The hash of the initial ProRegTx.\n"
        },
        {votingAddress_register,
            "%d. \"votingAddress\"          (string, required) The voting key address. The private key does not have to be known by your wallet.\n"
            "                                 It has to match the private key which is later used when voting on proposals.\n"
            "                                 If set to an empty string, ownerAddress will be used.\n"
        },
        {votingAddress_update,
            "%d. \"votingAddress\"          (string, required) The voting key address. The private key does not have to be known by your wallet.\n"
            "                                 It has to match the private key which is later used when voting on proposals.\n"
            "                                 If set to an empty string, the currently active voting key address is reused.\n"
        },
    };

std::string GetHelpString(int nParamNum, ProRegParam p)
{
    auto it = mapParamHelp.find(p);
    if (it == mapParamHelp.end())
        throw std::runtime_error(strprintf("FIXME: WRONG PARAM: %d!", (int)p));

    return strprintf(it->second, nParamNum);
}

#ifdef ENABLE_WALLET
static CKey GetKeyFromWallet(CWallet* pwallet, const CKeyID& keyID)
{
    assert(pwallet);
    CKey key;
    if (!pwallet->GetKey(keyID, key)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,
                           strprintf("key for address %s not in wallet", EncodeDestination(keyID)));
    }
    return key;
}
#endif

// Allows to specify PIVX address or priv key (as strings). In case of PIVX address, the priv key is taken from the wallet
static CKey ParsePrivKey(CWallet* pwallet, const std::string &strKeyOrAddress, bool allowAddresses = true) {
    bool isStaking{false}, isShield{false};
    const CWDestination& cwdest = Standard::DecodeDestination(strKeyOrAddress, isStaking, isShield);
    if (isStaking) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "cold staking addresses not supported");
    }
    if (isShield) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "shield addresses not supported");
    }
    const CTxDestination* dest = Standard::GetTransparentDestination(cwdest);
    if (allowAddresses && IsValidDestination(*dest)) {
#ifdef ENABLE_WALLET
        if (!pwallet) {
            throw std::runtime_error("addresses not supported when wallet is disabled");
        }
        EnsureWalletIsUnlocked(pwallet);
        const CKeyID* keyID = boost::get<CKeyID>(dest);
        assert (keyID != nullptr);  // we just checked IsValidDestination
        return GetKeyFromWallet(pwallet, *keyID);
#else   // ENABLE_WALLET
        throw std::runtime_error("addresses not supported in no-wallet builds");
#endif  // ENABLE_WALLET
    }

    CKey key = DecodeSecret(strKeyOrAddress);
    if (!key.IsValid()) throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key encoding");
    return key;
}

static CKeyID ParsePubKeyIDFromAddress(const std::string& strAddress)
{
    bool isStaking{false}, isShield{false};
    const CWDestination& cwdest = Standard::DecodeDestination(strAddress, isStaking, isShield);
    if (isStaking) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "cold staking addresses not supported");
    }
    if (isShield) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "shield addresses not supported");
    }
    const CKeyID* keyID = boost::get<CKeyID>(Standard::GetTransparentDestination(cwdest));
    if (!keyID) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, strprintf("invalid PIVX address %s", strAddress));
    }
    return *keyID;
}

#ifdef ENABLE_WALLET

template<typename SpecialTxPayload>
static void FundSpecialTx(CWallet* pwallet, CMutableTransaction& tx, SpecialTxPayload& payload)
{
    assert(pwallet != nullptr);
    SetTxPayload(tx, payload);

    static CTxOut dummyTxOut(0, CScript() << OP_RETURN);
    std::vector<CRecipient> vecSend;
    bool dummyTxOutAdded = false;

    if (tx.vout.empty()) {
        // add dummy txout as CreateTransaction requires at least one recipient
        tx.vout.emplace_back(dummyTxOut);
        dummyTxOutAdded = true;
    }

    CAmount nFee;
    CFeeRate feeRate = CFeeRate(0);
    int nChangePos = -1;
    std::string strFailReason;
    std::set<int> setSubtractFeeFromOutputs;
    if (!pwallet->FundTransaction(tx, nFee, false, feeRate, nChangePos, strFailReason, false, false))
        throw JSONRPCError(RPC_INTERNAL_ERROR, strFailReason);

    if (dummyTxOutAdded && tx.vout.size() > 1) {
        // FundTransaction added a change output, so we don't need the dummy txout anymore
        // Removing it results in slight overpayment of fees, but we ignore this for now (as it's a very low amount)
        auto it = std::find(tx.vout.begin(), tx.vout.end(), dummyTxOut);
        assert(it != tx.vout.end());
        tx.vout.erase(it);
    }

    UpdateSpecialTxInputsHash(tx, payload);
}

template<typename SpecialTxPayload>
static void UpdateSpecialTxInputsHash(const CMutableTransaction& tx, SpecialTxPayload& payload)
{
    payload.inputsHash = CalcTxInputsHash(tx);
}

template<typename SpecialTxPayload>
static void SignSpecialTxPayloadByHash(const CMutableTransaction& tx, SpecialTxPayload& payload, const CKey& key)
{
    payload.vchSig.clear();

    uint256 hash = ::SerializeHash(payload);
    if (!CHashSigner::SignHash(hash, key, payload.vchSig)) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "failed to sign special tx payload");
    }
}

template<typename SpecialTxPayload>
static void SignSpecialTxPayloadByString(SpecialTxPayload& payload, const CKey& key)
{
    payload.vchSig.clear();

    std::string m = payload.MakeSignString();
    if (!CMessageSigner::SignMessage(m, payload.vchSig, key)) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "failed to sign special tx payload");
    }
}

static std::string SignAndSendSpecialTx(CMutableTransaction& tx, const ProRegPL& pl)
{
    EnsureWallet();
    EnsureWalletIsUnlocked();

    SetTxPayload(tx, pl);

    CValidationState state;
    if (!CheckSpecialTx(tx, GetChainTip(), state)) {
        throw std::runtime_error(FormatStateMessage(state));
    }

    CDataStream ds(SER_NETWORK, PROTOCOL_VERSION);
    ds << tx;

    JSONRPCRequest signRequest;
    signRequest.params.setArray();
    signRequest.params.push_back(HexStr(ds.begin(), ds.end()));
    UniValue signResult = signrawtransaction(signRequest);

    if (!signResult["complete"].get_bool()) {
        std::ostringstream ss;
        ss << "failed to sign special tx: ";
        UniValue errors = signResult["errors"].get_array();
        for (unsigned int i = 0; i < errors.size(); i++) {
            if (i > 0) ss << " - ";
            ss << errors[i].get_str();
        }
        throw JSONRPCError(RPC_INTERNAL_ERROR, ss.str());
    }

    JSONRPCRequest sendRequest;
    sendRequest.params.setArray();
    sendRequest.params.push_back(signResult["hex"].get_str());
    return sendrawtransaction(sendRequest).get_str();
}

// Parses inputs (starting from index paramIdx) and returns ProReg payload
static ProRegPL ParseProRegPLParams(const UniValue& params, unsigned int paramIdx)
{
    assert(pwalletMain);
    assert(params.size() > paramIdx + 4);
    assert(params.size() < paramIdx + 8);
    ProRegPL pl;

    // ip and port
    const std::string& strIpPort = params[paramIdx].get_str();
    if (!strIpPort.empty()) {
        if (!Lookup(strIpPort.c_str(), pl.addr, Params().GetDefaultPort(), false)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("invalid network address %s", strIpPort));
        }
    }

    // addresses/keys
    const std::string& strAddOwner = params[paramIdx + 1].get_str();
    const std::string& strAddOperator = params[paramIdx + 2].get_str();
    const std::string& strAddVoting = params[paramIdx + 3].get_str();
    pl.keyIDOwner = ParsePubKeyIDFromAddress(strAddOwner);
    pl.keyIDOperator = pl.keyIDOwner;
    if (!strAddOperator.empty()) {
        pl.keyIDOperator = ParsePubKeyIDFromAddress(strAddOperator);
    }
    pl.keyIDVoting = pl.keyIDOwner;
    if (!strAddVoting.empty()) {
        pl.keyIDVoting = ParsePubKeyIDFromAddress(strAddVoting);
    }

    // payout script (!TODO: add support for P2CS)
    const std::string& strAddPayee = params[paramIdx + 4].get_str();
    pl.scriptPayout = GetScriptForDestination(CTxDestination(ParsePubKeyIDFromAddress(strAddPayee)));

    // operator reward
    pl.nOperatorReward = 0;
    if (params.size() > paramIdx + 5) {
        int64_t operReward = 0;
        if (!ParseFixedPoint(params[paramIdx + 5].getValStr(), 2, &operReward)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "operatorReward must be a number");
        }
        if (operReward < 0 || operReward > 10000) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "operatorReward must be between 0.00 and 100.00");
        }
        pl.nOperatorReward = (uint16_t)operReward;
        if (params.size() > paramIdx + 6) {
            // operator reward payout script
            const std::string& strAddOpPayee = params[paramIdx + 6].get_str();
            if (pl.nOperatorReward > 0 && !strAddOpPayee.empty()) {
                pl.scriptOperatorPayout = GetScriptForDestination(CTxDestination(ParsePubKeyIDFromAddress(strAddOpPayee)));
            } else if (!strAddOpPayee.empty()) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "operatorPayoutAddress must be empty when operatorReward is 0");
            }
        }
    }
    return pl;
}

// handles protx_register, and protx_register_prepare
static UniValue ProTxRegister(const JSONRPCRequest& request, bool fSignAndSend)
{
    if (request.fHelp || request.params.size() < 7 || request.params.size() > 9) {
        throw std::runtime_error(
                (fSignAndSend ?
                    "protx_register \"collateralHash\" collateralIndex \"ipAndPort\" \"ownerAddress\" \"operatorAddress\" \"votingAddress\" \"payoutAddress\" (operatorReward \"operatorPayoutAddress\")\n"
                    "The collateral is specified through \"collateralHash\" and \"collateralIndex\" and must be an unspent\n"
                    "transaction output spendable by this wallet. It must also not be used by any other masternode.\n"
                        :
                    "protx_register_prepare \"collateralHash\" collateralIndex \"ipAndPort\" \"ownerAddress\" \"operatorAddress\" \"votingAddress\" \"payoutAddress\" (operatorReward \"operatorPayoutAddress\")\n"
                    "\nCreates an unsigned ProTx and returns it. The ProTx must be signed externally with the collateral\n"
                    "key and then passed to \"protx register_submit\".\n"
                    "The collateral is specified through \"collateralHash\" and \"collateralIndex\" and must be an unspent transaction output.\n"
                )
                + HelpRequiringPassphrase() + "\n"
                "\nArguments:\n"
                + GetHelpString(1, collateralHash)
                + GetHelpString(2, collateralIndex)
                + GetHelpString(3, ipAndPort_register)
                + GetHelpString(4, ownerAddress)
                + GetHelpString(5, operatorAddress_register)
                + GetHelpString(6, votingAddress_register)
                + GetHelpString(7, payoutAddress_register)
                + GetHelpString(8, operatorReward)
                + GetHelpString(9, operatorPayoutAddress_register) +
                "\nResult:\n" +
                (fSignAndSend ? (
                        "\"txid\"                 (string) The transaction id.\n"
                        "\nExamples:\n"
                        + HelpExampleCli("protx_register", "...!TODO...")
                        ) : (
                        "{                        (json object)\n"
                        "  \"tx\" :                 (string) The serialized ProTx in hex format.\n"
                        "  \"collateralAddress\" :  (string) The collateral address.\n"
                        "  \"signMessage\" :        (string) The string message that needs to be signed with the collateral key\n"
                        "}\n"
                        "\nExamples:\n"
                        + HelpExampleCli("protx_register_prepare", "...!TODO...")
                        )
                )
        );
    }

    EnsureWallet();
    EnsureWalletIsUnlocked();
    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwalletMain->BlockUntilSyncedToCurrentChain();

    const uint256& collateralHash = ParseHashV(request.params[0], "collateralHash");
    const int32_t collateralIndex = request.params[1].get_int();
    if (collateralIndex < 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("invalid collateral index (negative): %d", collateralIndex));
    }

    ProRegPL pl = ParseProRegPLParams(request.params, 2);
    pl.nVersion = ProRegPL::CURRENT_VERSION;
    pl.collateralOutpoint = COutPoint(collateralHash, (uint32_t)collateralIndex);

    CMutableTransaction tx;
    tx.nVersion = CTransaction::TxVersion::SAPLING;
    tx.nType = CTransaction::TxType::PROREG;

    // referencing unspent collateral outpoint
    Coin coin;
    {
        LOCK(cs_main);
        if (!GetUTXOCoin(pl.collateralOutpoint, coin)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("collateral not found: %s-%d", collateralHash.ToString(), collateralIndex));
        }
    }
    if (coin.out.nValue != MN_COLL_AMT) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("collateral %s-%d with invalid value %d", collateralHash.ToString(), collateralIndex, coin.out.nValue));
    }
    CTxDestination txDest;
    ExtractDestination(coin.out.scriptPubKey, txDest);
    const CKeyID* keyID = boost::get<CKeyID>(&txDest);
    if (!keyID) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("collateral type not supported: %s-%d", collateralHash.ToString(), collateralIndex));
    }
    CKey keyCollateral;
    if (fSignAndSend && !pwalletMain->GetKey(*keyID, keyCollateral)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, strprintf("collateral key not in wallet: %s", EncodeDestination(txDest)));
    }

    // make sure fee calculation works
    pl.vchSig.resize(CPubKey::COMPACT_SIGNATURE_SIZE);

    FundSpecialTx(pwalletMain, tx, pl);

    if (fSignAndSend) {
        SignSpecialTxPayloadByString(pl, keyCollateral); // prove we own the collateral
        // check the payload, add the tx inputs sigs, and send the tx.
        return SignAndSendSpecialTx(tx, pl);
    }
    // external signing with collateral key
    pl.vchSig.clear();
    SetTxPayload(tx, pl);
    UniValue ret(UniValue::VOBJ);
    ret.pushKV("tx", EncodeHexTx(tx));
    ret.pushKV("collateralAddress", EncodeDestination(txDest));
    ret.pushKV("signMessage", pl.MakeSignString());
    return ret;
}

UniValue protx_register(const JSONRPCRequest& request)
{
    return ProTxRegister(request, true);
}

UniValue protx_register_prepare(const JSONRPCRequest& request)
{
    return ProTxRegister(request, false);
}

UniValue protx_register_submit(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2) {
        throw std::runtime_error(
                "protx register_submit \"tx\" \"sig\"\n"
                "\nSubmits the specified ProTx to the network. This command will also sign the inputs of the transaction\n"
                "which were previously added by \"protx register_prepare\" to cover transaction fees\n"
                + HelpRequiringPassphrase() + "\n"
                "\nArguments:\n"
                "1. \"tx\"                 (string, required) The serialized transaction previously returned by \"protx register_prepare\"\n"
                "2. \"sig\"                (string, required) The signature signed with the collateral key. Must be in base64 format.\n"
                "\nResult:\n"
                "\"txid\"                  (string) The transaction id.\n"
                "\nExamples:\n"
                + HelpExampleCli("protx", "register_submit \"tx\" \"sig\"")
        );
    }

    EnsureWallet();
    EnsureWalletIsUnlocked();
    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwalletMain->BlockUntilSyncedToCurrentChain();

    CMutableTransaction tx;
    if (!DecodeHexTx(tx, request.params[0].get_str())) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "transaction not deserializable");
    }
    if (tx.nType != CTransaction::TxType::PROREG) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "transaction not a ProRegTx");
    }
    ProRegPL pl;
    if (!GetTxPayload(tx, pl)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "transaction payload not deserializable");
    }
    if (!pl.vchSig.empty()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "payload signature not empty");
    }

    pl.vchSig = DecodeBase64(request.params[1].get_str().c_str());

    // check the payload, add the tx inputs sigs, and send the tx.
    return SignAndSendSpecialTx(tx, pl);
}

UniValue protx_register_fund(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 6 || request.params.size() > 8) {
        throw std::runtime_error(
                "protx_register_fund \"collateralAddress\" \"ipAndPort\" \"ownerAddress\" \"operatorAddress\" \"votingAddress\" \"payoutAddress\" (operatorReward \"operatorPayoutAddress\")\n"
                "\nCreates, funds and sends a ProTx to the network. The resulting transaction will move 10000 PIV\n"
                "to the address specified by collateralAddress and will then function as masternode collateral.\n"
                + HelpRequiringPassphrase() + "\n"
                "\nArguments:\n"
                + GetHelpString(1, collateralAddress)
                + GetHelpString(2, ipAndPort_register)
                + GetHelpString(3, ownerAddress)
                + GetHelpString(4, operatorAddress_register)
                + GetHelpString(5, votingAddress_register)
                + GetHelpString(6, payoutAddress_register)
                + GetHelpString(7, operatorReward)
                + GetHelpString(8, operatorPayoutAddress_register) +
                "\nResult:\n"
                "\"txid\"                        (string) The transaction id.\n"
                "\nExamples:\n"
                + HelpExampleCli("protx_register_fund", "...!TODO...")
        );
    }

    EnsureWallet();
    EnsureWalletIsUnlocked();
    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwalletMain->BlockUntilSyncedToCurrentChain();

    const CTxDestination& collateralDest(ParsePubKeyIDFromAddress(request.params[0].get_str()));
    const CScript& collateralScript = GetScriptForDestination(collateralDest);

    ProRegPL pl = ParseProRegPLParams(request.params, 1);
    pl.nVersion = ProRegPL::CURRENT_VERSION;

    CMutableTransaction tx;
    tx.nVersion = CTransaction::TxVersion::SAPLING;
    tx.nType = CTransaction::TxType::PROREG;
    tx.vout.emplace_back(MN_COLL_AMT, collateralScript);

    FundSpecialTx(pwalletMain, tx, pl);

    for (uint32_t i = 0; i < tx.vout.size(); i++) {
        if (tx.vout[i].nValue == MN_COLL_AMT && tx.vout[i].scriptPubKey == collateralScript) {
            pl.collateralOutpoint.n = i;
            break;
        }
    }
    assert(pl.collateralOutpoint.n != (uint32_t) -1);
    // update payload on tx (with final collateral outpoint)
    pl.vchSig.clear();
    // check the payload, add the tx inputs sigs, and send the tx.
    return SignAndSendSpecialTx(tx, pl);
}

#endif  //ENABLE_WALLET


static const CRPCCommand commands[] =
{ //  category       name                              actor (function)
  //  -------------- --------------------------------- ------------------------
#ifdef ENABLE_WALLET
    { "evo",         "protx_register",                 &protx_register,         {}  },
    { "evo",         "protx_register_fund",            &protx_register_fund,    {}  },
    { "evo",         "protx_register_prepare",         &protx_register_prepare, {}  },
    { "evo",         "protx_register_submit",          &protx_register_submit,  {}  },
#endif  //ENABLE_WALLET
};

void RegisterEvoRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++) {
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
    }
}
