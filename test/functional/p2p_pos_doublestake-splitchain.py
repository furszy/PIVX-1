#!/usr/bin/env python3
# Copyright (c) 2019 The PIVX Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

'''
Covers the scenario of two valid PoS blocks with same height
and same coinstake input.
'''

from copy import deepcopy
import time
from test_framework.util import bytes_to_hex_str
from fake_stake.base_test import PIVX_FakeStakeTest


class PoSDoubleStakeSplitChain(PIVX_FakeStakeTest):

    def run_test(self):
        self.description = "Covers the scenario of two valid PoS blocks with same height and same coinstake input."
        self.init_test()
        INITAL_MINED_BLOCKS = 300

        # 1) Starting mining blocks
        self.log.info("Mining %d blocks.." % INITAL_MINED_BLOCKS)
        self.node.generate(INITAL_MINED_BLOCKS)

        # 2) Collect 3 prevouts
        self.log.info("Collecting all unspent coins which we generated from mining...")
        staking_utxo_list = self.node.listunspent()[:3]

        # 3) Create the two blocks

        # -- A) get latest  block number and hash
        block_count = self.node.getblockcount()
        pastBlockHash = self.node.getblockhash(block_count)
        self.log.info("Block count: %d - Current best: %s" % (self.node.getblockcount(), self.node.getbestblockhash()))

        # -- B) collect prevouts used for staking
        stakingPrevOuts = self.get_prevouts(staking_utxo_list, block_count)

        # -- C) get first block and get a copy of it
        block_A = self.create_spam_block(pastBlockHash, stakingPrevOuts, block_count+1)
        self.log.info("Hash of Block A: %s" % block_A.hash)
        block_B = deepcopy(block_A)

        # -- D) remove last spending tx in block_B and rehash it
        _ = block_B.vtx.pop()
        block_B.hashMerkleRoot = block_B.calc_merkle_root()
        block_B.rehash()
        block_B.sign_block(self.block_sig_key)
        self.log.info("Hash of Block B: %s" % block_B.hash)

        # 4) Try broadcasting blocks
        self.log.info("Testing blocks")
        self.test_nodes[0].handle_connect()

        self.log.info("sending block A (%s)" % block_A.hash)
        try:
            var = self.node.submitblock(bytes_to_hex_str(block_A.serialize()))
            time.sleep(2)
            bestHash = self.node.getbestblockhash()
            self.log.info("Block count: %d - Current best: %s" % (self.node.getblockcount(), bestHash))
            if var is not None:
                self.log.info("result: %s" % str(var))
        except Exception as e:
            self.log.info("Exception: %s" % str(e))

        self.log.info("sending block B (%s)" % block_B.hash)
        try:
            var = self.node.submitblock(bytes_to_hex_str(block_B.serialize()))
            time.sleep(2)
            bestHash = self.node.getbestblockhash()
            self.log.info("Block count: %d - Current best: %s" % (self.node.getblockcount(), bestHash))
            if var is not None:
                self.log.info("result: %s" % str(var))
        except Exception as e:
            self.log.info("Exception: %s" % str(e))

        # 5) Recollect prevouts
        self.log.info("Recollecting unspent utxos...")
        staking_utxo_list = self.node.listunspent()

        # 6) Append another block to the forked chain
        block_count += 1
        pastBlockHash = block_B.hash if bestHash == block_A.hash else block_A.hash
        stakingPrevOuts = self.get_prevouts(staking_utxo_list, block_count)

        block_C = self.create_spam_block(pastBlockHash, stakingPrevOuts, block_count + 1)
        self.log.info("Hash of Block C: %s" % block_C.hash)
        try:
            var = self.node.submitblock(bytes_to_hex_str(block_C.serialize()))
            time.sleep(2)
            self.log.info("Current tip: %d [besthash: %s]" % (
                self.node.getblockcount(), bestHash))
            if var is not None:
                self.log.info("result: %s" % str(var))
        except Exception as e:
            self.log.info("Exception: %s" % str(e))




if __name__ == '__main__':
    PoSDoubleStakeSplitChain().main()
