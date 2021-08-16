/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fs435@cornell.edu>
 *                Matthew Burke <matthelb@cs.cornell.edu>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************************/
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
  virtual void Execute(AsyncTransaction *txn, execute_callback ecb) = 0;

  inline const Stats &GetStats() const { return stats; }
 protected:
  Stats stats;
};

#endif /* ASYNC_CLIENT_H */
