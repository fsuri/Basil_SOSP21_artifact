#include "store/common/frontend/transaction_utils.h"

Operation Get(const std::string &key) {
  return Operation{GET, key, ""};
}

Operation Put(const std::string &key,
    const std::string &value) {
  return Operation{PUT, key, value};
}

Operation Commit() {
  return Operation{COMMIT, "", ""};
}

Operation Abort() {
  return Operation{ABORT, "", ""};
}

