#include "store/common/frontend/async_transaction.h"

AsyncTransaction::AsyncTransaction() : opCount(0UL), finished(false),
    committed(false) {
}

AsyncTransaction::AsyncTransaction(const AsyncTransaction &txn) :
  opCount(txn.opCount), readValues(txn.readValues) {
}

AsyncTransaction::~AsyncTransaction() {
}

void AsyncTransaction::GetReadValue(const std::string &key,
    std::string &value, bool &found) const {
  auto itr = readValues.find(key);
  if (itr == readValues.end()) {
    found = false;
  } else {
    found = true;
    // copy here
    value = itr->second;
  }
}
