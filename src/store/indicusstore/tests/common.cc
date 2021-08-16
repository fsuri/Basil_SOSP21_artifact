/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fs435@cornell.edu>
 *                Matthew Burke <matthelb@cs.cornell.edu>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************************/
#include "store/indicusstore/tests/common.h"

#include "store/indicusstore/common.h"

namespace indicusstore {

void GenerateTestConfig(int g, int f, std::stringstream &ss) {
  int n = 5 * f + 1;
  ss << "f " << f << std::endl;
  for (int i = 0; i < g; ++i) {
    ss << "group" << std::endl;
    for (int j = 0; j < n; ++j) {
      ss << "replica localhost:8000" << std::endl;
    }
  }
}

void PopulateTransaction(const std::map<std::string, Timestamp> &readSet,
    const std::map<std::string, std::string> &writeSet, const Timestamp &ts,
    const std::set<int> &involvedGroups, proto::Transaction &txn) {
  txn.set_client_id(1);
  txn.set_client_seq_num(1);
  for (const auto group : involvedGroups) {
    txn.add_involved_groups(group);
  }
  for (const auto &read : readSet) {
    ReadMessage *readMsg = txn.add_read_set();
    readMsg->set_key(read.first);
    read.second.serialize(readMsg->mutable_readtime());
  }
  for (const auto &write : writeSet) {
    WriteMessage *writeMsg = txn.add_write_set();
    writeMsg->set_key(write.first);
    writeMsg->set_value(write.second);
  }
  ts.serialize(txn.mutable_timestamp());
}

void PopulateCommitProof(proto::CommittedProof &proof, int n) {
  proof.mutable_txn()->set_client_id(1);
  proof.mutable_txn()->set_client_seq_num(1);
  proof.mutable_txn()->add_involved_groups(0);
  ReadMessage *read = proof.mutable_txn()->add_read_set();
  read->set_key("1");
  read->mutable_readtime()->set_timestamp(50);
  read->mutable_readtime()->set_id(2);
  proof.mutable_txn()->mutable_timestamp()->set_timestamp(100);
  proof.mutable_txn()->mutable_timestamp()->set_id(1);
  for (int i = 0; i < n; ++i) {
    proto::Phase2Reply *p2Reply = proof.mutable_p2_replies()->add_replies();
    p2Reply->set_req_id(3);
    p2Reply->set_decision(proto::COMMIT);
    *p2Reply->mutable_txn_digest() = TransactionDigest(proof.txn());
  }
}

} // namespace indicusstore
