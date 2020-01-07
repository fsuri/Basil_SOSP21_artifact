#include "store/mortystore/branch_generator.h"

#include <sstream>

#include "lib/message.h"
#include "store/mortystore/common.h"

#include <google/protobuf/util/message_differencer.h>

namespace mortystore {

BranchGenerator::BranchGenerator() {
}

BranchGenerator::~BranchGenerator() {
}

void BranchGenerator::AddActive(const proto::Branch &branch) {
  for (size_t i = 0; i < branch.txn().ops_size(); ++i) {
    const proto::Operation &op = branch.txn().ops(i);
    if (op.type() == proto::OperationType::READ) {
      AddActiveRead(op.key(), branch);
    } else {
      AddActiveWrite(op.key(), branch);
    }
  }
}

void BranchGenerator::AddActiveWrite(const std::string &key,
    const proto::Branch &branch) {
  active_writes[key][branch.txn().id()].insert(branch);
}

void BranchGenerator::AddActiveRead(const std::string &key,
    const proto::Branch &branch) {
  active_reads[key][branch.txn().id()].insert(branch);
}

void BranchGenerator::ClearActive(uint64_t txn_id) {
  for (auto &kv : active_writes) {
    kv.second.erase(txn_id);
  }
  for (auto &kv : active_reads) {
    kv.second.erase(txn_id);
  }
}

} // namespace mortystore
