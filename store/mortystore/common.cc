#include "store/mortystore/common.h"

namespace mortystore {

void PrintBranch(const proto::Branch &branch) {
  for (auto b : branch.seq()) {
    std::cerr << b.txn().id() << "[";
    for (auto o : b.txn().ops()) {
      if (o.type() == proto::OperationType::READ) {
        std::cerr << "r(" << o.key() << "),";
      } else {
        std::cerr << "w(" << o.key() << "),";
      }
    }
    std::cerr << "],";
  }
  std::cerr << branch.txn().id() << "[";
  for (auto o : branch.txn().ops()) {
    if (o.type() == proto::OperationType::READ) {
      std::cerr << "r(" << o.key() << "),";
    } else {
      std::cerr << "w(" << o.key() << "),";
    }
  }
  std::cerr << "]" << std::endl;
}

} // namespace mortystore
