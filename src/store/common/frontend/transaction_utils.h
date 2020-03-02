#ifndef TRANSACTION_UTILS_H
#define TRANSACTION_UTILS_H

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

Operation Get(const std::string &key);

Operation Put(const std::string &key,
    const std::string &value);

Operation Commit();

Operation Abort();

#endif
