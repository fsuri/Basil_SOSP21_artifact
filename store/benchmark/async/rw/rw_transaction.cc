#include "store/benchmark/async/rw/rw_transaction.h"

namespace rw {

RWTransaction::RWTransaction(KeySelector *keySelector,
    int numKeys) : keySelector(keySelector) {
  for (int i = 0; i < numKeys; ++i) {
    keyIdxs.push_back(keySelector->GetKey());
  }
}

RWTransaction::~RWTransaction() {
}

Operation RWTransaction::GetNextOperation(size_t opCount,
    std::map<std::string, std::string> readValues) {
  if (opCount < 2 * GetNumKeys()) {
    if (opCount % 2 == 0) {
      return Get(GetKey(opCount / 2));
    } else {
      return Put(GetKey(opCount / 2), std::to_string(opCount));
    }
  } else {
    return Commit();
  }
}

} // namespace rw
