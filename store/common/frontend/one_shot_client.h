#ifndef ONE_SHOT_CLIENT_H
#define ONE_SHOT_CLIENT_H

#include "store/common/frontend/async_transaction.h"
#include "store/common/frontend/one_shot_transaction.h"

class OneShotClient {
 public:
  OneShotClient() {};
  virtual ~OneShotClient() {};

  // Begin a transaction.
  virtual void Execute(OneShotTransaction *txn, execute_callback ecb) = 0;

};

#endif /* ONE_SHOT_CLIENT_H */
