#ifndef _SYNC_TRANSACTION_H_
#define _SYNC_TRANSACTION_H_

#include "store/common/frontend/client.h"
#include "store/common/frontend/transaction_utils.h"

#include <functional>
#include <map>
#include <string>

class SyncTransaction {
 public:
  SyncTransaction(uint32_t timeout) : timeout(timeout) { }
  virtual ~SyncTransaction() { }

  virtual transaction_status_t Execute(SyncClient &client) = 0;

 protected:
  const uint32_t timeout;
};

#endif
