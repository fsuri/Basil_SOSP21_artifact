#ifndef ONE_SHOT_CLIENT_H
#define ONE_SHOT_CLIENT_H

#include "store/common/frontend/async_transaction.h"

#include <functional>

#define SUCCESS 0

typedef std::function<void(int, std::map<std::string, std::string>)> execute_callback;

class OneShotClient {
 public:
  OneShotClient() {};
  virtual ~OneShotClient() {};

  // Begin a transaction.
  virtual void Execute(OneShotTransaction *txn, execute_callback ecb) = 0;

};

#endif /* ONE_SHOT_CLIENT_H */
