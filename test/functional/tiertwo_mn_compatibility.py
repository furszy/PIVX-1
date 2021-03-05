#!/usr/bin/env python3
# Copyright (c) 2021 The PIVX developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import PivxTier2TestFramework
from test_framework.util import (
    assert_equal,
)

from decimal import Decimal

"""
Test checking compatibility code between MN and DMN
"""

class MasternodeCompatibilityTest(PivxTier2TestFramework):

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 7
        self.enable_mocktime()

        self.minerPos = 0
        self.ownerOnePos = self.ownerTwoPos = 1
        self.remoteOnePos = 2
        self.remoteTwoPos = 3
        self.remoteDMN1Pos = 4
        self.remoteDMN2Pos = 5
        self.remoteDMN3Pos = 6

        self.masternodeOneAlias = "mnOne"
        self.masternodeTwoAlias = "mntwo"

        self.extra_args = [["-nuparams=v5_shield:249", "-nuparams=v6_evo:250"]] * self.num_nodes
        for i in [self.remoteOnePos, self.remoteTwoPos, self.remoteDMN1Pos, self.remoteDMN2Pos, self.remoteDMN3Pos]:
            self.extra_args[i] += ["-listen", "-externalip=127.0.0.1"]
        self.extra_args[self.minerPos].append("-sporkkey=932HEevBSujW2ud7RfB1YF91AFygbBRQj3de3LyaCRqNzKKgWXi")

        self.mnOnePrivkey = "9247iC59poZmqBYt9iDh9wDam6v9S1rW5XekjLGyPnDhrDkP4AK"
        self.mnTwoPrivkey = "92Hkebp3RHdDidGZ7ARgS4orxJAGyFUPDXNqtsYsiwho1HGVRbF"

        self.miner = None
        self.ownerOne = self.ownerTwo = None
        self.remoteOne = None
        self.remoteTwo = None
        self.remoteDMN1 = None
        self.remoteDMN2 = None
        self.remoteDMN3 = None

    def check_mns_status_legacy(self, node, txhash):
        status = node.getmasternodestatus()
        assert_equal(status["txhash"], txhash)
        assert_equal(status["message"], "Masternode successfully started")

    def check_mns_status(self, node, txhash):
        status = node.getmasternodestatus()
        assert_equal(status["proTxHash"], txhash)
        assert_equal(status["dmnstate"]["PoSePenalty"], 0)
        assert_equal(status["status"], "Ready")

    def check_mn_list(self, node, txHashSet):
        # check masternode list from node
        mnlist = node.listmasternodes()
        assert_equal(len(mnlist), len(txHashSet))
        foundHashes = set([mn["txhash"] for mn in mnlist if mn["txhash"] in txHashSet])
        if len(foundHashes) != len(txHashSet):
            raise Exception(str(mnlist))
        for x in mnlist:
            self.mn_addresses.add(x["addr"])
        self.log.info("MN address list has %d entries" % len(self.mn_addresses))

    def run_test(self):
        self.mn_addresses = set()
        self.enable_mocktime()
        self.setup_3_masternodes_network()

        # add two more nodes to the network
        self.remoteDMN2 = self.nodes[self.remoteDMN2Pos]
        self.remoteDMN3 = self.nodes[self.remoteDMN3Pos]

        # check mn list from miner
        txHashSet = set([self.mnOneCollateral.hash, self.mnTwoCollateral.hash, self.proRegTx1.hash])
        self.check_mn_list(self.miner, txHashSet)

        # check status of masternodes
        self.check_mns_status_legacy(self.remoteOne, self.mnOneCollateral.hash)
        self.log.info("MN1 active")
        self.check_mns_status_legacy(self.remoteTwo, self.mnTwoCollateral.hash)
        self.log.info("MN2 active")
        self.check_mns_status(self.remoteDMN1, self.proRegTx1.hash)
        self.log.info("DMN1 active")

        # Create another DMN, this time without funding the collateral.
        # ProTx references another transaction in the owner's wallet
        self.proRegTx2, self.dmn2Privkey = self.setupDMN(
            self.ownerOne,
            self.miner,
            self.remoteDMN2Pos,
            "internal"
        )
        self.remoteDMN2.initmasternode(self.dmn2Privkey, "", True)

        # check list and status
        txHashSet.add(self.proRegTx2.hash)
        self.check_mn_list(self.miner, txHashSet)
        self.check_mns_status(self.remoteDMN2, self.proRegTx2.hash)
        self.log.info("DMN2 active")

        # Check block version and coinbase payment
        blk_count = self.miner.getblockcount()
        self.log.info("Checking block version and coinbase payment...")
        blk = self.miner.getblock(self.miner.getbestblockhash(), True)
        assert_equal(blk['height'], blk_count)
        assert_equal(blk['version'], 10)
        cbase_tx = self.miner.getrawtransaction(blk['tx'][0], True)
        assert_equal(len(cbase_tx['vin']), 1)
        cbase_script = blk_count.to_bytes(1 + blk_count//256, byteorder="little")
        cbase_script = len(cbase_script).to_bytes(1, byteorder="little") + cbase_script + bytearray(1)
        assert_equal(cbase_tx['vin'][0]['coinbase'], cbase_script.hex())
        assert_equal(len(cbase_tx['vout']), 1)
        assert_equal(cbase_tx['vout'][0]['value'], Decimal("3.0"))
        payee = cbase_tx['vout'][0]['scriptPubKey']['addresses'][0]
        if payee not in self.mn_addresses:
            raise Exception("payee %s not found in expected list %s" % (payee, str(self.mn_addresses)))
        cstake_tx = self.miner.getrawtransaction(blk['tx'][1], True)
        assert_equal(len(cstake_tx['vin']), 1)
        assert_equal(len(cstake_tx['vout']), 2)
        assert_equal(cstake_tx['vout'][1]['value'], Decimal("497.0")) # 250 + 250 - 3
        self.log.info("Block at height %d checks out" % blk_count)

        # Now create a DMN, reusing the collateral output of a legacy MN
        self.log.info("Creating a DMN reusing the collateral of a legacy MN...")
        self.proRegTx3, self.dmn3Privkey = self.setupDMN(
            self.ownerOne,
            self.miner,
            self.remoteDMN3Pos,
            "external",
            self.mnOneCollateral,
        )
        self.remoteDMN3.initmasternode(self.dmn3Privkey, "", True)

        # The remote node is shutting down the pinging service
        try:
            self.send_3_pings()
        except:
            pass

        # The legacy masternode must no longer be in the list
        # and the DMN must have taken its place
        txHashSet.remove(self.mnOneCollateral.hash)
        txHashSet.add(self.proRegTx3.hash)
        for node in self.nodes:
            self.check_mn_list(node, txHashSet)
        self.log.info("Masternode list correctly updated by all nodes.")
        self.check_mns_status(self.remoteDMN3, self.proRegTx3.hash)
        self.log.info("DMN3 active")

        # Now try to start a legacy MN with a collateral used by a DMN
        self.log.info("Now trying to start a legacy MN with a collateral of a DMN...")
        self.controller_start_masternode(self.ownerOne, self.masternodeOneAlias)
        try:
            self.send_3_pings()
        except:
            pass

        # the masternode list hasn't changed
        for node in self.nodes:
            self.check_mn_list(node, txHashSet)
        self.log.info("Masternode list correctly unchanged in all nodes.")

if __name__ == '__main__':
    MasternodeCompatibilityTest().main()
