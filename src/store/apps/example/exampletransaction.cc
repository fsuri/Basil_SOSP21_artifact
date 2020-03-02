#include "store/apps/example/exampletransaction.h"

ExampleTransaction::ExampleTransaction() {
}

ExampleTransaction::~ExampleTransaction() {
}

/**
 * Begin()
 * v = Get('a') + Get('b') + Get('c)
 * Put('d', v)
 * Commit()
 */

OperationType ExampleTransaction::GetNextOperationType() {
  switch (GetOpCount()) {
    case 0:
      return GET;
    case 1:
      return GET;
    case 2:
      return GET;
    case 3:
      return PUT;
    case 4:
      return COMMIT;
    default:
      return DONE;
  }
}

void ExampleTransaction::GetNextOperationKey(std::string &key) {
  switch (GetOpCount()) {
    case 0:
      key = "a";
      break;
    case 1:
      key = "b";
      break;
    case 2:
      key = "c";
      break;
    case 3:
      key = "d";
      break;
    default:
      break;
  }
}

void ExampleTransaction::GetNextPutValue(std::string &value) {
  std::string rv;
  bool found;

  value = "";

  GetReadValue("a", rv, found);
  UW_ASSERT(found);
  value = value + rv;
  
  GetReadValue("b", rv, found);
  UW_ASSERT(found);
  value = value + rv;

  GetReadValue("c", rv, found);
  UW_ASSERT(found);
  value = value + rv;
}

void ExampleTransaction::OnOperationCompleted(const Operation *op,
    ::Client *client) {
}

void ExampleTransaction::CopyStateInto(AsyncTransaction *txn) const {
}
