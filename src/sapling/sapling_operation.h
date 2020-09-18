// Copyright (c) 2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_SAPLING_OPERATION_H
#define PIVX_SAPLING_OPERATION_H

#include "amount.h"
#include "sapling/transaction_builder.h"
#include "operationresult.h"
#include "primitives/transaction.h"
#include "wallet/wallet.h"

// transaction.h comment: spending taddr output requires CTxIn >= 148 bytes and typical taddr txout is 34 bytes
#define CTXIN_SPEND_DUST_SIZE   148
#define CTXOUT_REGULAR_SIZE     34

struct TxValues;

class SendManyRecipient {
public:
    const std::string address;
    const CAmount amount;
    const std::string memo;

    SendManyRecipient(const std::string& address_, CAmount amount_, std::string memo_) :
            address(address_), amount(amount_), memo(memo_) {}
};

class FromAddress {
public:
    explicit FromAddress() {};
    explicit FromAddress(const CTxDestination& _fromTaddr) : fromTaddr(_fromTaddr) {};
    explicit FromAddress(const libzcash::SaplingPaymentAddress& _fromSapaddr) : fromSapAddr(_fromSapaddr) {};

    bool isFromTAddress() const { return IsValidDestination(fromTaddr); }
    bool isFromSapAddress() const { return fromSapAddr.is_initialized(); }

    CTxDestination fromTaddr{CNoDestination()};
    Optional<libzcash::SaplingPaymentAddress> fromSapAddr{nullopt};
};

class SaplingOperation {
public:
    explicit SaplingOperation(const Consensus::Params& consensusParams, int chainHeight) : txBuilder(consensusParams, chainHeight) {};
    explicit SaplingOperation(TransactionBuilder& _builder) : txBuilder(_builder) {};

    ~SaplingOperation() { delete tkeyChange; }

    OperationResult build();
    OperationResult send(std::string& retTxHash);
    OperationResult buildAndSend(std::string& retTxHash);

    void setFromAddress(const CTxDestination&);
    void setFromAddress(const libzcash::SaplingPaymentAddress&);
    // In case of no addressFrom filter selected, it will accept any utxo in the wallet as input.
    SaplingOperation* setSelectTransparentCoins(const bool select) { selectFromtaddrs = select; return this; };
    SaplingOperation* setSelectShieldedCoins(const bool select) { selectFromShield = select; return this; };
    SaplingOperation* setTransparentRecipients(std::vector<SendManyRecipient>& vec) { taddrRecipients = std::move(vec); return this; };
    SaplingOperation* setShieldedRecipients(std::vector<SendManyRecipient>& vec) { shieldedAddrRecipients = std::move(vec); return this; } ;
    SaplingOperation* setFee(CAmount _fee) { fee = _fee; return this; }
    SaplingOperation* setMinDepth(int _mindepth) { assert(_mindepth >= 0); mindepth = _mindepth; return this; }
    SaplingOperation* setTxBuilder(TransactionBuilder& builder) { txBuilder = builder; return this; }
    SaplingOperation* setTransparentKeyChange(CReserveKey* reserveKey) { tkeyChange = reserveKey; return this; }

    CTransaction getFinalTx() { return finalTx; }

    // Public only for unit test coverage
    bool getMemoFromHexString(const std::string& s, std::array<unsigned char, ZC_MEMO_SIZE> memoRet, std::string& error);

    // Test only
    bool testMode{false};

private:
    FromAddress fromAddress;
    // In case of no addressFrom filter selected, it will accept any utxo in the wallet as input.
    bool selectFromtaddrs{false};
    bool selectFromShield{false};
    std::vector<SendManyRecipient> taddrRecipients;
    std::vector<SendManyRecipient> shieldedAddrRecipients;
    std::vector<COutput> transInputs;
    std::vector<SaplingNoteEntry> shieldedInputs;
    int mindepth{5}; // Min default depth 5.
    CAmount fee{DEFAULT_SAPLING_FEE}; // Hardcoded fee for now.

    // transparent change
    CReserveKey* tkeyChange{nullptr};

    // Builder
    TransactionBuilder txBuilder;
    CTransaction finalTx;

    OperationResult loadUtxos(TxValues& values);
    OperationResult loadUnspentNotes(TxValues& txValues,
                                     libzcash::SaplingExpandedSpendingKey& expsk,
                                     uint256& ovk);
    OperationResult checkTxValues(TxValues& txValues, bool isFromtAddress, bool isFromShielded);
};

OperationResult CheckTransactionSize(std::vector<SendManyRecipient>& shieldedRecipients,
                                     bool fromTaddr, int tAddrRecipientsSize);

#endif //PIVX_SAPLING_OPERATION_H
