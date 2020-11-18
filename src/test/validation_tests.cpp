// Copyright (c) 2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "test/test_pivx.h"
#include "primitives/transaction.h"
#include "tiertwo/specialtx_validation.h"
#include "test/librust/utiltest.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(validation_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(special_tx_validation_test)
{
    // First check, sapling not active, transaction with extra payload
    CMutableTransaction mtx;
    mtx.extraPayload = std::vector<uint8_t>(10, 1);
    CValidationState state;
    BOOST_CHECK(!CheckSpecialTx(CTransaction(mtx), state, false));

    // Now activate sapling
    RegtestActivateSapling();

    // After Sapling activation.
    // v1 can only be Type=0
    mtx.nType = 1;
    mtx.nVersion = CTransaction::TxVersion::LEGACY;
    BOOST_CHECK(!CheckSpecialTx(CTransaction(mtx), state, true));
    BOOST_CHECK(state.GetRejectReason().find("not supported with version 0"));

    // version >= Sapling, type = 0, payload != null.
    mtx.nType = 0;
    mtx.nVersion = CTransaction::TxVersion::SAPLING;
    BOOST_CHECK(!CheckSpecialTx(CTransaction(mtx), state, true));
    BOOST_CHECK(state.GetRejectReason().find("desn't support extra payload"));

    // version >= Sapling, type = 0, payload == null --> pass
    mtx.extraPayload = nullopt;
    BOOST_CHECK(CheckSpecialTx(CTransaction(mtx), state, true));

    // nVersion>=2 and nType!=0 without extrapayload
    mtx.nType = 1;
    BOOST_CHECK(!CheckSpecialTx(CTransaction(mtx), state, true));
    BOOST_CHECK(state.GetRejectReason().find("without extra payload"));

    // Size limits
    mtx.extraPayload = std::vector<uint8_t>(10001, 1);
    BOOST_CHECK(!CheckSpecialTx(CTransaction(mtx), state, true));
    BOOST_CHECK(state.GetRejectReason().find("Special tx payload oversize"));

    // Remove two elements, so now it passes the size check
    mtx.extraPayload->pop_back();
    mtx.extraPayload->pop_back();
    BOOST_CHECK(!CheckSpecialTx(CTransaction(mtx), state, true));
    BOOST_CHECK(state.GetRejectReason().find("with invalid type"));

    RegtestDeactivateSapling();
}

BOOST_AUTO_TEST_SUITE_END()
