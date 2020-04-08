#ifndef ASYNC_TPCC_TRANSACTION_H
#define ASYNC_TPCC_TRANSACTION_H

#include "store/common/frontend/async_transaction.h"

namespace tpcc {

class AsyncTPCCTransaction : public AsyncTransaction {
 public:
  AsyncTPCCTransaction();
  virtual ~AsyncTPCCTransaction();
};

}

#endif /* ASYNC_TPCC_TRANSACTION_H */
