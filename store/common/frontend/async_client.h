#ifndef ASYNC_CLIENT_H
#define ASYNC_CLIENT_H

#include "store/common/frontend/async_transaction.h"

#include <functional>

typedef std::function<void(int, std::map<std::string, std::string>)> execute_callback;


class AsyncClient {
 public:
  AsyncClient() {};
  virtual ~AsyncClient() {};

  // Begin a transaction.
  virtual void Execute(AsyncTransaction *txn, execute_callback ecb) = 0;

};

#endif /* ASYNC_CLIENT_H */
