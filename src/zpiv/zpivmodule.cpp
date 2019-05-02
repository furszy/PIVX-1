// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zpiv/zpivmodule.h"
#include "zpivchain.h"
#include "libzerocoin/Commitment.h"
#include "libzerocoin/Coin.h"
#include "hash.h"
#include "iostream"

bool PublicCoinSpend::HasValidSerial(libzerocoin::ZerocoinParams* params) const {
    return IsValidSerial(params, coinSerialNumber);
}

bool PublicCoinSpend::HasValidSignature() const {
    // Now check that the signature validates with the serial
    try {
        //V2 serial requires that the signature hash be signed by the public key associated with the serial
        uint256 hashedPubkey = Hash(pubkey.begin(), pubkey.end()) >> libzerocoin::PrivateCoin::V2_BITSHIFT;
        if (hashedPubkey != libzerocoin::GetAdjustedSerial(coinSerialNumber).getuint256()) {
            return error("%s: adjusted serial invalid\n", __func__);
        }
    } catch(std::range_error &e) {
        throw libzerocoin::InvalidSerialException("Serial longer than 256 bits");
    }

    if (!pubkey.Verify(ptxHash, vchSig)){
        std::cout << "pubkey not verified: key: " << pubkey.GetHash().GetHex() <<  std::endl;
        std::cout << "hash tx out: " << ptxHash.GetHex() << std::endl;
        //std::cout << "signature: " << vchSig << std::endl;
        return error("%s: adjusted serial invalid\n", __func__);
    }
    return true;
}

bool ZPIVModule::createInput(CTxIn &in, CZerocoinMint& mint, uint256 hashTxOut){
    libzerocoin::ZerocoinParams* params = Params().Zerocoin_Params(false);
    uint8_t nVersion = mint.GetVersion();
    if (nVersion < libzerocoin::PrivateCoin::PUBKEY_VERSION) {
        // No v1 serials accepted anymore.
        return error("%s: failed to set zPIV privkey mint version=%d\n", __func__, nVersion);
    }
    CKey key;
    if (!mint.GetKeyPair(key))
        return error("%s: failed to set zPIV privkey mint version=%d\n", __func__, nVersion);

    std::vector<unsigned char> vchSig;
    if (!key.Sign(hashTxOut, vchSig))
        throw std::runtime_error("ZPIVModule failed to sign hashTxOut\n");

    CDataStream ser(SER_NETWORK, PROTOCOL_VERSION);
    PublicCoinSpend spend(params, mint.GetSerialNumber(),  mint.GetRandomness(), key.GetPubKey(), vchSig);
    ser << spend;

    std::vector<unsigned char> data(ser.begin(), ser.end());
    CScript scriptSigIn = CScript() << OP_ZEROCOINPUBLICSPEND << data.size();
    scriptSigIn.insert(scriptSigIn.end(), data.begin(), data.end());
    in = CTxIn(mint.GetTxHash(), mint.GetOutputIndex(), scriptSigIn, mint.GetDenomination());
    in.nSequence = mint.GetDenomination();
    return true;
}

bool ZPIVModule::parseCoinSpend(const CTxIn &in, const CTransaction& tx, const CTxOut &prevOut, PublicCoinSpend& publicCoinSpend){
    if (!in.scriptSig.IsZerocoinPublicSpend()) throw runtime_error("parseCoinSpend() :: input is not a public coin spend");
    std::vector<char, zero_after_free_allocator<char> > data;
    data.insert(data.end(), in.scriptSig.begin() + 4, in.scriptSig.end());
    CDataStream serializedCoinSpend(data, SER_NETWORK, PROTOCOL_VERSION);
    libzerocoin::ZerocoinParams* params = Params().Zerocoin_Params(false);
    PublicCoinSpend spend(params, serializedCoinSpend);

    spend.outputIndex = in.prevout.n;
    spend.txHash = in.prevout.hash;
    CMutableTransaction txNew(tx);
    txNew.vin.clear();
    spend.setTxOutHash(txNew.GetHash());

    // Check prev out now
    CValidationState state;
    if (!TxOutToPublicCoin(prevOut, spend.pubCoin, state))
        return error("%s: cannot get mint from output\n", __func__);

    spend.setDenom(spend.pubCoin.getDenomination());
    publicCoinSpend = spend;
    return true;
}

bool ZPIVModule::validateInput(const CTxIn &in, const CTxOut &prevOut, const CTransaction& tx, PublicCoinSpend& publicSpend){
    if (!in.scriptSig.IsZerocoinPublicSpend() || !prevOut.scriptPubKey.IsZerocoinMint())
        return error("%s: not valid argument/s\n", __func__);

    // Now prove that the commitment value opens to the input
    if (!parseCoinSpend(in, tx, prevOut, publicSpend)){
        return false;
    }
    // TODO: Validate that the prev out has the same spend denom??
    libzerocoin::ZerocoinParams* params = Params().Zerocoin_Params(false);
    // Check that it opens to the input values
    libzerocoin::Commitment commitment(
            &params->coinCommitmentGroup, publicSpend.getCoinSerialNumber(), publicSpend.randomness);
    if (commitment.getCommitmentValue() != publicSpend.pubCoin.getValue()){
        std::cout << "invalid commitment" << std::endl;
        return error("%s: commitments values are not equal\n", __func__);
    }
    // Now check that the signature validates with the serial
    if (!publicSpend.HasValidSignature()) {
        return error("%s: signature invalid\n", __func__);;
    }

    return true;
}