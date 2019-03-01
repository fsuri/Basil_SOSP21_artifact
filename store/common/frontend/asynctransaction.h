#ifndef _ASYNC_TRANSACTION_H_
#define _ASYNC_TRANSACTION_H_

#include "store/common/frontend/asyncclient.h"

#include <functional>
#include <map>
#include <string>

enum OperationType {
  GET,
  PUT,
  COMMIT,
  ABORT,
  DONE,
  NUM_OPERATION_TYPES
};

struct Operation {
  Operation(size_t id, OperationType type) : id(id), type(type) { }
  size_t id;
  OperationType type;
};

struct GetOperation : Operation {
  GetOperation(size_t id, OperationType type, const std::string &key,
      const std::string &value) : Operation(id, type), key(key), value(value) {
  }
  const std::string &key;
  const std::string &value;
};

struct PutOperation : Operation {
  PutOperation(size_t id, OperationType type, const std::string &key) :
      Operation(id, type), key(key) { }
  const std::string &key;
};

struct CommitAbortOperation : public Operation {
  CommitAbortOperation(size_t id, OperationType type, bool committed) :
      Operation(id, type), committed(committed) { }
  bool committed;
};

class AsyncTransaction {
 public:
  AsyncTransaction();
  AsyncTransaction(const AsyncTransaction &txn);
  virtual ~AsyncTransaction();

  void Execute(AsyncClient *client);

 protected:
  virtual OperationType GetNextOperationType() = 0;
  virtual void GetNextOperationKey(std::string &key) = 0;
  virtual void GetNextPutValue(std::string &value) = 0;
  virtual void OnOperationCompleted(const Operation *op) = 0;
  virtual void CopyStateInto(AsyncTransaction *txn) const = 0;

  inline size_t GetOpCount() const { return opCount; }
  void GetReadValue(const std::string &key, std::string &value,
      bool &found) const;
  

 private:
  void ExecuteNextOperation(AsyncClient *client);

  size_t opCount;
  std::map<std::string, std::string> readValues;
};

#endif
