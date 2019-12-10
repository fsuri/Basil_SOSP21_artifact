#include "store/benchmark/async/rw/rw_transaction.h"

namespace rw {

RWTransaction::RWTransaction(KeySelector *keySelector, int numOps,
    std::mt19937 &rand) : keySelector(keySelector), numOps(numOps) {
  for (int i = 0; i < numOps; ++i) {
    uint64_t key;
    if (i % 2 == 0) {
      key = keySelector->GetKey(rand);
    } else {
      key = keyIdxs[i - 1];
    }
    keyIdxs.push_back(key);
  }
}

RWTransaction::~RWTransaction() {
}

Operation RWTransaction::GetNextOperation(size_t opCount,
    std::map<std::string, std::string> readValues) {
  if (opCount < GetNumOps()) {
    if (opCount % 2 == 0) {
      return Get(GetKey(opCount));
    } else {
      return Put(GetKey(opCount), std::to_string(opCount));
    }
  } else {
    return Commit();
  }
}

} // namespace rw
