#include "store/indicusstore/tests/common.h"

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
    proto::Transaction &txn) {
  txn.set_client_id(1);
  txn.set_client_seq_num(1);
  txn.add_involved_groups(0);
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

} // namespace indicusstore
