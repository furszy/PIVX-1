// Copyright (c) 2015-2020 The Zcash developers
// Copyright (c) 2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_SAPLING_CORE_WRITE_H
#define PIVX_SAPLING_CORE_WRITE_H

#include "primitives/transaction.h"
#include <univalue.h>

// Format Sapling tx information in json.
void TxSaplingToJSON(const CTransaction& tx, UniValue& entry);

// Format shielded spends information in json.
UniValue TxShieldedSpendsToJSON(const CTransaction& tx);

// Format shielded outputs information in json.
UniValue TxShieldedOutputsToJSON(const CTransaction& tx);

#endif //PIVX_SAPLING_CORE_WRITE_H
