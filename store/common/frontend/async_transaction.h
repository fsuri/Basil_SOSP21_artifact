#ifndef _ASYNC_TRANSACTION_H_
#define _ASYNC_TRANSACTION_H_

#include "store/common/frontend/client.h"

#include <functional>
#include <map>
#include <string>

enum OperationType {
  GET,
  PUT,
  COMMIT,
  ABORT
};

struct Operation {
  OperationType type;
  std::string key;
  std::string value;
};

class AsyncTransaction {
 public:
  AsyncTransaction(uint64_t tid) : tid(tid) { }
  virtual ~AsyncTransaction() { }

  virtual Operation GetNextOperation(size_t opCount,
      const std::map<std::string, std::string> readValues) = 0;

 protected:
  inline static Operation Get(const std::string &key) {
    return Operation{GET, key, ""};
  }
  inline static Operation Put(const std::string &key,
      const std::string &value) {
    return Operation{PUT, key, value};
  }
  inline static Operation Commit() {
    return Operation{COMMIT, "", ""};
  }
  inline static Operation Abort() {
    return Operation{ABORT, "", ""};
  }
 private:
  uint64_t tid;

};

#endif
