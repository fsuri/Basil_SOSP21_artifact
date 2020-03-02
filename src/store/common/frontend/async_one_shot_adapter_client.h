#ifndef ASYNC_ONE_SHOT_ADAPTER_CLIENT_H
#define ASYNC_ONE_SHOT_ADAPTER_CLIENT_H

#include "store/common/frontend/async_client.h"
#include "store/common/frontend/one_shot_client.h"

class AsyncOneShotAdapterClient : public AsyncClient {
 public:
  AsyncOneShotAdapterClient(OneShotClient *client);
  virtual ~AsyncOneShotAdapterClient();

  // Begin a transaction.
  virtual void Execute(AsyncTransaction *txn, execute_callback ecb);

 private:
  void ExecuteNextOperation();
  void GetCallback(int status, const std::string &key, const std::string &val,
      Timestamp ts);
  void GetTimeout(int status, const std::string &key);
  void PutCallback(int status, const std::string &key, const std::string &val);
  void PutTimeout(int status, const std::string &key, const std::string &val);
  void CommitCallback(int result);
  void CommitTimeout(int status);
  void AbortCallback();
  void AbortTimeout(int status);

  OneShotClient *client;
  size_t opCount;
  std::map<std::string, std::string> readValues;
  execute_callback currEcb;
  AsyncTransaction *currTxn;

};

#endif /* ASYNC_ONE_SHOT_ADAPTER_CLIENT_H */
