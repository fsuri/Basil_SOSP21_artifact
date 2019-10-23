#include "store/mortystore/common.h"

#include <google/protobuf/util/message_differencer.h>

bool operator==(const mortystore::proto::Branch &b1,
    const mortystore::proto::Branch &b2) {
  return b1.txn().id() == b2.txn().id()
    || google::protobuf::util::MessageDifferencer::Equals(b1, b2);
}

bool operator==(const mortystore::proto::Transaction &t1,
    const mortystore::proto::Transaction &t2) {
  return t1.id() == t2.id()
    || google::protobuf::util::MessageDifferencer::Equals(t1, t2);
}

namespace mortystore {

size_t BranchHasher::operator() (const proto::Branch &b) const {
  return b.txn().id();
}

bool BranchComparer::operator() (const proto::Branch &b1, const proto::Branch &b2) const {
  return b1 == b2;
}

void PrintBranch(const proto::Branch &branch) {
  for (const proto::Transaction &b : branch.seq()) {
    std::cerr << b.id() << "[";
    for (const proto::Operation &o : b.ops()) {
      if (o.type() == proto::OperationType::READ) {
        std::cerr << "r(" << o.key() << "),";
      } else {
        std::cerr << "w(" << o.key() << "),";
      }
    }
    std::cerr << "],";
  }
  std::cerr << branch.txn().id() << "[";
  for (const proto::Operation &o : branch.txn().ops()) {
    if (o.type() == proto::OperationType::READ) {
      std::cerr << "r(" << o.key() << "),";
    } else {
      std::cerr << "w(" << o.key() << "),";
    }
  }
  std::cerr << "]" << std::endl;
}

} // namespace mortystore
