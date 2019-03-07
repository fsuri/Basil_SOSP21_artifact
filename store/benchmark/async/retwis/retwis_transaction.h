#ifndef RETWIS_TRANSACTION_H
#define RETWIS_TRANSACTION_H

#include <vector>

#include "store/common/frontend/async_transaction.h"
#include "store/common/frontend/client.h"

namespace retwis {

class RetwisTransaction : public AsyncTransaction {
 public:
  RetwisTransaction(std::function<std::string()> chooseKey, int numKeys);
  virtual ~RetwisTransaction();

 protected:
  inline const std::string &GetKey(int i) const { return keys[i]; }
  inline size_t GetNumKeys() const { return keys.size(); }

 private:
  std::vector<std::string> keys;

};

}

#endif /* RETWIS_TRANSACTION_H */
