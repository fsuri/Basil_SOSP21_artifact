#ifndef RETWIS_TRANSACTION_H
#define RETWIS_TRANSACTION_H

#include <vector>

#include "store/common/frontend/asynctransaction.h"
#include "store/common/frontend/client.h"

namespace retwis {

class RetwisTransaction : public AsyncTransaction {
 public:
  RetwisTransaction(std::function<int()> chooseKey, int numKeys);
  virtual ~RetwisTransaction();

 protected:
  inline int GetOpsCompleted() const { return 0; }
  inline int GetKey(int i) const { return keys[i]; }

 private:
  std::vector<int> keys;

};

}

#endif /* RETWIS_TRANSACTION_H */
