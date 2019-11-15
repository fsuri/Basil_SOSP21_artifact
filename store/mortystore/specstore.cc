#include "store/mortystore/specstore.h"

namespace mortystore {

SpecStore::SpecStore() {
}

SpecStore::~SpecStore() {
}

bool SpecStore::get(const std::string &key, const proto::Transaction &reader,
    std::string &val) {
  auto itr = store.find(key);
  if (itr == store.end()) {
    return false;
  } else {
    val = itr->second.value;
    itr->second.mrr.push_back(reader);
    return true;
  }
}

void SpecStore::put(const std::string &key, const std::string &val,
    const proto::Transaction &writer) {
  SpecValue &sv = store[key];
  sv.value = val;
  sv.mrw = writer;
}

bool SpecStore::MostRecentConflict(const proto::Operation &op,
      const proto::Transaction *&txn) const {
  auto itr = store.find(op.key());
  if (itr == store.end()) {
    return false;
  } else {
    if (op.type() == proto::OperationType::READ || itr->second.mrr.size() == 0) {
      txn = &itr->second.mrw;
    } else {
      txn = &itr->second.mrr[itr->second.mrr.size() - 1];
    }
    return true;
  }
}


} // namespace mortystore
