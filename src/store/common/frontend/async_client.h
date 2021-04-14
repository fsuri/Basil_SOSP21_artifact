#ifndef ASYNC_CLIENT_H
#define ASYNC_CLIENT_H

#include "store/common/frontend/client.h"
#include "store/common/frontend/async_transaction.h"
#include "store/common/stats.h"

#include <functional>

#define SUCCESS 0
#define FAILED 1
typedef std::function<void(transaction_status_t, std::map<std::string, std::string>)> execute_callback;

class AsyncClient {
 public:
  AsyncClient() {};
  virtual ~AsyncClient() {};

  // Begin a transaction.
  virtual void Execute(AsyncTransaction *txn, execute_callback ecb, bool retry=false) = 0;

  inline const Stats &GetStats() const { return stats; }
 protected:
  Stats stats;
};

#endif /* ASYNC_CLIENT_H */
