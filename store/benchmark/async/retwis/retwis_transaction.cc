#include "store/benchmark/async/retwis/retwis_transaction.h"

namespace retwis {

RetwisTransaction::RetwisTransaction(uint64_t tid, Client *client,
    KeySelector *keySelector, int numKeys)
    : AsyncTransaction(tid, client), keySelector(keySelector) {
  for (int i = 0; i < numKeys; ++i) {
    keyIdxs.push_back(keySelector->GetKey());
  }
}

RetwisTransaction::~RetwisTransaction() {
}

} // namespace retwis
