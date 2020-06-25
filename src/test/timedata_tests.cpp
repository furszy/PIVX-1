// Copyright (c) 2011-2014 The Bitcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "timedata.h"
#include "test/test_pivx.h"

#include <boost/test/unit_test.hpp>


BOOST_FIXTURE_TEST_SUITE(timedata_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(util_MedianFilter)
{
    CMedianFilter<int> filter(5, 15);

    BOOST_CHECK_EQUAL(filter.median(), 15);

    filter.input(20); // [15 20]
    BOOST_CHECK_EQUAL(filter.median(), 17);

    filter.input(30); // [15 20 30]
    BOOST_CHECK_EQUAL(filter.median(), 20);

    filter.input(3); // [3 15 20 30]
    BOOST_CHECK_EQUAL(filter.median(), 17);

    filter.input(7); // [3 7 15 20 30]
    BOOST_CHECK_EQUAL(filter.median(), 15);

    filter.input(18); // [3 7 18 20 30]
    BOOST_CHECK_EQUAL(filter.median(), 18);

    filter.input(0); // [0 3 7 18 30]
    BOOST_CHECK_EQUAL(filter.median(), 7);
}

BOOST_AUTO_TEST_CASE(timeOffset)
{
    SelectParams(CBaseChainParams::REGTEST);
    const int nTimeSlotLength = Params().GetConsensus().nTimeSlotLength;

    CMedianFilter<int64_t> vTimeOffsets1(200, 0);
    int64_t curTime = GetTime();
    // First test, "all good".
    for (int i = 0; i < 16; ++i) {
        int64_t nTimeOffset = curTime - GetTime();
        vTimeOffsets1.input(nTimeOffset);
    }
    BOOST_CHECK(CheckTimeOffset(vTimeOffsets1, nTimeSlotLength));

    // Second test, node time behind network
    CMedianFilter<int64_t> vTimeOffsets2(200, 0);
    for (int i = 0; i < 16; ++i) {
        int64_t nTimeOffset = curTime - nTimeSlotLength - GetTime();
        vTimeOffsets2.input(nTimeOffset);
    }
    BOOST_CHECK(!CheckTimeOffset(vTimeOffsets2, nTimeSlotLength));

    // Second test, node time above network time
    CMedianFilter<int64_t> vTimeOffsets3(200, 0);
    for (int i = 0; i < 16; ++i) {
        int64_t nTimeOffset = curTime + nTimeSlotLength - GetTime();
        vTimeOffsets3.input(nTimeOffset);
    }
    BOOST_CHECK(!CheckTimeOffset(vTimeOffsets3, nTimeSlotLength));
}

BOOST_AUTO_TEST_SUITE_END()
