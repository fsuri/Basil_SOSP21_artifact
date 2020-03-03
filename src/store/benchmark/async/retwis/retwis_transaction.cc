#include "store/benchmark/async/retwis/retwis_transaction.h"

namespace retwis {

RetwisTransaction::RetwisTransaction(KeySelector *keySelector,
    int numKeys, std::mt19937 &rand) : keySelector(keySelector) {
  for (int i = 0; i < numKeys; ++i) {
    keyIdxs.push_back(keySelector->GetKey(rand));
  }
}

RetwisTransaction::~RetwisTransaction() {
}

} // namespace retwis
