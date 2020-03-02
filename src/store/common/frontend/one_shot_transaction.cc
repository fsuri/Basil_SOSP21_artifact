#include "store/common/frontend/one_shot_transaction.h"

OneShotTransaction::OneShotTransaction() {
}

OneShotTransaction::~OneShotTransaction() {
}

void OneShotTransaction::AddRead(const std::string &key) {
  read_set.insert(key);
}

void OneShotTransaction::AddWrite(const std::string &key,
    const std::string &value) {
  write_set.insert(std::make_pair(key, value));
}

