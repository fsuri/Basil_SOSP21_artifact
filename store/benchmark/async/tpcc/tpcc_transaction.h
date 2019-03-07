#ifndef TPCC_TRANSACTION_H
#define TPCC_TRANSACTION_H

#include <vector>

#include "store/common/frontend/async_transaction.h"
#include "store/common/frontend/client.h"

namespace tpcc {

class TPCCTransaction : public AsyncTransaction {
 public:
  TPCCTransaction();
  virtual ~TPCCTransaction();

};

}

#endif /* TPCC_TRANSACTION_H */
