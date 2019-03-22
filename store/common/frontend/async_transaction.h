#ifndef _ASYNC_TRANSACTION_H_
#define _ASYNC_TRANSACTION_H_

#include "store/common/frontend/client.h"

#include <functional>
#include <map>
#include <string>

typedef std::function<void(bool, std::map<std::string, std::string>)> execute_callback;

class AsyncTransactionShim {
 public:
  AsyncTransactionShim() { }
  virtual ~AsyncTransactionShim() { }


};

class AsyncTransaction {
 public:
  AsyncTransaction(uint64_t tid, Client *client);
  AsyncTransaction(const AsyncTransaction &txn);
  virtual ~AsyncTransaction();

  void Execute(execute_callback ecb);
  virtual void ExecuteNextOperation() = 0;

 protected:
  void CopyStateInto(AsyncTransaction *txn) const {};

  inline size_t GetOpsCompleted() const { return opCount; }

  void Get(const std::string &key);
  void Put(const std::string &key, const std::string &value);
  void Commit();
  void Abort();

  void GetReadValue(const std::string &key, std::string &value,
      bool &found) const;

 private:
  void GetCallback(int status, const std::string &key, const std::string &val,
      Timestamp ts);
  void GetTimeout(int status, const std::string &key);
  void PutCallback(int status, const std::string &key, const std::string &val);
  void PutTimeout(int status, const std::string &key, const std::string &val);
  void CommitCallback(bool committed);
  void CommitTimeout(int status);
  void AbortCallback();
  void AbortTimeout(int status);

  uint64_t tid;
  Client *client;
  size_t opCount;
  std::map<std::string, std::string> readValues;
  execute_callback currEcb;
  AsyncTransactionShim *currShim;

};

#endif
