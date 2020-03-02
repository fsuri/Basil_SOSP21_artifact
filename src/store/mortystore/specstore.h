#ifndef SPEC_STORE_H
#define SPEC_STORE_H

#include "store/mortystore/morty-proto.pb.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace mortystore {

class SpecStore {
 public:
  SpecStore();
  virtual ~SpecStore();

  bool get(const std::string &key, const proto::Transaction &reader,
      std::string &val);
  void put(const std::string &key, const std::string &val,
      const proto::Transaction &writer);

  bool MostRecentConflict(const proto::Operation &op,
      const proto::Transaction *&txn) const;

  void ApplyTransaction(const proto::Transaction &txn);
 private:
  struct SpecValue {
    std::string value;
    proto::Transaction mrw;
    std::vector<proto::Transaction> mrr;
  };

  std::unordered_map<std::string, SpecValue> store;

};

} // namespace mortystore

#endif /* SPEC_STORE_H */
