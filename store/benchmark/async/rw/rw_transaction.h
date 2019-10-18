#ifndef RW_TRANSACTION_H
#define RW_TRANSACTION_H

#include <vector>

#include "store/common/frontend/async_transaction.h"
#include "store/common/frontend/client.h"
#include "store/benchmark/async/common/key_selector.h"

namespace rw {

class RWTransaction : public AsyncTransaction {
 public:
  RWTransaction(KeySelector *keySelector, int numKeys);
  virtual ~RWTransaction();

  virtual Operation GetNextOperation(size_t opCount,
      std::map<std::string, std::string> readValues);
 protected:
  inline const std::string &GetKey(int i) const {
    return keySelector->GetKey(keyIdxs[i]);
  }

  inline const size_t GetNumKeys() const { return keyIdxs.size(); } 

  KeySelector *keySelector;

 private:
  std::vector<int> keyIdxs;

};

}

#endif /* RW_TRANSACTION_H */
