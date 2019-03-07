#ifndef _ASYNC_TRANSACTION_H_
#define _ASYNC_TRANSACTION_H_

#include "store/common/frontend/asyncclient.h"
#include "store/common/frontend/client.h"

#include <functional>
#include <map>
#include <string>

class AsyncTransaction {
 public:
  AsyncTransaction();
  AsyncTransaction(const AsyncTransaction &txn);
  virtual ~AsyncTransaction();

  virtual void ExecuteNextOperation(Client *client) = 0;

  inline bool IsFinished() const { return finished; }
  inline bool IsCommitted() const { return committed; }

 protected:
  void CopyStateInto(AsyncTransaction *txn) const {};

  inline size_t GetOpsCompleted() const { return opCount; }
  inline void SetFinished(bool f) { finished = f; }
  inline void SetCommitted(bool c) { committed = c; }

  void GetReadValue(const std::string &key, std::string &value,
      bool &found) const;

 private:
  size_t opCount;
  bool finished;
  bool committed;
  std::map<std::string, std::string> readValues;

};

#endif
