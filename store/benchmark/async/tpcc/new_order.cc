#include "store/benchmark/async/tpcc/new_order.h"

namespace tpcc {

NewOrder::NewOrder(Client *client) : TPCCTransaction(client) {
}

NewOrder::~NewOrder() {
}

void NewOrder::ExecuteNextOperation() {
  if (GetOpsCompleted() == 0) {
    Get("hello");
  } else if (GetOpsCompleted() == 1) {
    Put("hello", "world");
  } else {
    Commit();
  }
}

}
