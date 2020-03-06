#ifndef MORTY_STORE_H
#define MORTY_STORE_H

#include "lib/assert.h"
#include "lib/message.h"
#include "store/common/timestamp.h"
#include "store/common/transaction.h"
#include "store/common/backend/txnstore.h"
#include "store/common/backend/versionstore.h"

#include <set>
#include <unordered_map>

namespace mortystore {

class Store {
 public:
  Store();
  virtual ~Store();

 private:
  // Data store
  VersionedKVStore<Timestamp, std::string> store;
};

} // namespace mortystore

#endif /* MORTY_STORE_H */
