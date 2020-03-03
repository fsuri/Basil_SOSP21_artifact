#ifndef _EXAMPLE_TRANSACTION_H_
#define _EXAMPLE_TRANSACTION_H_

#include "store/common/frontend/asynctransaction.h"

#include <set>

class ExampleTransaction : public AsyncTransaction {
 public:
  ExampleTransaction();
  ~ExampleTransaction();
 
 protected:
  OperationType GetNextOperationType() = 0;
  void GetNextOperationKey(std::string &key) = 0;
  void GetNextPutValue(std::string &value) = 0;
  void OnOperationCompleted(const Operation *op, ::Client *client) = 0;
  void CopyStateInto(AsyncTransaction *txn) const = 0;

};

#endif
