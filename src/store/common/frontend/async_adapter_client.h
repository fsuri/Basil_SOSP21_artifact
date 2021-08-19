/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fsp@cs.cornell.edu>
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
#ifndef ASYNC_ADAPTER_CLIENT_H
#define ASYNC_ADAPTER_CLIENT_H

#include "store/common/frontend/async_client.h"

class AsyncAdapterClient : public AsyncClient {
 public:
  AsyncAdapterClient(Client *client, uint32_t timeout);
  virtual ~AsyncAdapterClient();

  // Begin a transaction.
  virtual void Execute(AsyncTransaction *txn, execute_callback ecb);

 private:
  void ExecuteNextOperation();
  void GetCallback(int status, const std::string &key, const std::string &val,
      Timestamp ts);
  void GetTimeout(int status, const std::string &key);
  void PutCallback(int status, const std::string &key, const std::string &val);
  void PutTimeout(int status, const std::string &key, const std::string &val);
  void CommitCallback(transaction_status_t result);
  void CommitTimeout();
  void AbortCallback();
  void AbortTimeout();

  Client *client;
  uint32_t timeout;
  size_t outstandingOpCount;
  size_t finishedOpCount;
  std::map<std::string, std::string> readValues;
  execute_callback currEcb;
  AsyncTransaction *currTxn;

};

#endif /* ASYNC_ADAPTER_CLIENT_H */
