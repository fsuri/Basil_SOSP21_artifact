#ifndef RW_TRANSACTION_H
#define RW_TRANSACTION_H

#include <vector>

#include "store/common/frontend/async_transaction.h"
#include "store/common/frontend/client.h"
#include "store/benchmark/async/common/key_selector.h"

namespace rw {

class RWTransaction : public AsyncTransaction {
 public:
  RWTransaction(KeySelector *keySelector, int numOps, std::mt19937 &rand);
  virtual ~RWTransaction();

  virtual Operation GetNextOperation(size_t outstandingOpCount, size_t finishedOpCount,
      std::map<std::string, std::string> readValues);

  inline const std::vector<int> getKeyIdxs() const {
    return keyIdxs;
  }
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

#endif /* RW_TRANSACTION_H */
