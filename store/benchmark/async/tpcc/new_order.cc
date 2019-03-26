#include "store/benchmark/async/tpcc/new_order.h"

namespace tpcc {

NewOrder::NewOrder() : TPCCTransaction() {
}

NewOrder::~NewOrder() {
}

Operation NewOrder::GetNextOperation(size_t opCount,
    const std::map<std::string, std::string> &readValues) {
  if (opCount == 0) {
    return Get("hello");
  } else if (opCount == 1) {
    return Put("hello", "world");
  } else {
    return Commit();
  }
}

}
