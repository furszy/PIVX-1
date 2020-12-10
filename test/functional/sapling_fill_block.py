#!/usr/bin/env python3
# Copyright (c) 2020 The PIVX developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import PivxTestFramework
from test_framework.util import (
    assert_equal,
    assert_true
)
import time

"""
Test to verify what happen if a big number of shield transactions gets into the mempool and are included in a single block.
Scenario: 900 transactions, only one shielded output each.
"""

class SaplingFillBlockTest(PivxTestFramework):

    def set_test_params(self):
        self.num_nodes = 3
        self.setup_clean_chain = True
        self.extra_args = [[], [], []]

    def generate_and_sync(self, count):
        assert(count > 0)
        height = self.nodes[0].getblockcount()
        self.nodes[0].generate(count)
        assert_equal(height + count, self.nodes[0].getblockcount())

    def run_test(self):
        # First mine 101 blocks to activate sapling
        self.log.info("Generating 2500 blocks...")
        self.generate_and_sync(2500)
        assert_equal(self.nodes[0].getblockchaininfo()['upgrades']['v5 shield']['status'], 'active')

        txNum = 900
        # Send 900 tx of 3 PIV to shield addr1
        self.log.info("Adding 900 shield txs right to the mempool...")
        z_addr1 = self.nodes[1].getnewshieldedaddress()
        txs = []
        for i in range(txNum):
            if (i % 10 == 0):
                self.log.info("shielding tx round: " + str(i))
            txs.append(self.nodes[0].shieldedsendmany("from_transparent", [{'address': z_addr1, 'amount': 3}], 1))
        self.sync_all()

        # let's see how big the mempool is now
        mempool = self.nodes[1].getrawmempool()

        for i in range(80):
            assert_true(txs[i] in mempool)
        assert_equal(self.nodes[1].getmempoolinfo()['size'], txNum)

        # Now let's try to mine it..
        startTime = time.time()
        self.log.info("mining block..")
        self.nodes[0].generate(1)
        self.log.info("block generation + processing time: " + str(time.time() - startTime))
        self.sync_all()
        self.log.info("peers synced, block mined!")
        self.nodes[0].generate(5)

if __name__ == '__main__':
    SaplingFillBlockTest().main()
