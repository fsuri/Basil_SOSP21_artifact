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
