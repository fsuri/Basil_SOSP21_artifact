#include "store/benchmark/async/retwis/retwis_transaction.h"

namespace retwis {

RetwisTransaction::RetwisTransaction(std::function<std::string()> chooseKey,
    int numKeys) {
  for (int i = 0; i < numKeys; ++i) {
    keys.push_back(chooseKey());
  }
}

RetwisTransaction::~RetwisTransaction() {
}

} // namespace retwis
