#ifndef RW_AUGUSTUS_TRANSACTION_H
#define RW_AUGUSTUS_TRANSACTION_H

#include <vector>

#include "store/common/frontend/async_transaction.h"
#include "store/common/frontend/client.h"
#include "store/benchmark/async/common/key_selector.h"

namespace rw_augustus {

class RWAugustusTransaction : public AsyncTransaction {
 public:
  RWAugustusTransaction(KeySelector *keySelector, int numOps, std::mt19937 &rand);
  virtual ~RWAugustusTransaction();

  virtual Operation GetNextOperation(size_t outstandingOpCount, size_t finishedOpCount,
      std::map<std::string, std::string> readValues);
 protected:
  inline const std::string &GetKey(int i) const {
    return keySelector->GetKey(keyIdxs[i]);
  }

  inline const size_t GetNumOps() const { return numOps; }

  KeySelector *keySelector;

 private:
  const size_t numOps;
  std::vector<int> keyIdxs;

};

}

#endif /* RW_AUGUSTUS_TRANSACTION_H */
