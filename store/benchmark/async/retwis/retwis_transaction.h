#ifndef RETWIS_TRANSACTION_H
#define RETWIS_TRANSACTION_H

#include <vector>

#include "store/common/frontend/async_transaction.h"
#include "store/common/frontend/client.h"
#include "store/benchmark/async/common/key_selector.h"

namespace retwis {

class RetwisTransaction : public AsyncTransaction {
 public:
  RetwisTransaction(Client *client, KeySelector *keySelector, int numKeys);
  virtual ~RetwisTransaction();

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

#endif /* RETWIS_TRANSACTION_H */
