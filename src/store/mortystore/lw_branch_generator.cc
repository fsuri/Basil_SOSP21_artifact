#include "store/mortystore/lw_branch_generator.h"

namespace mortystore {

LWBranchGenerator::LWBranchGenerator() {
}

LWBranchGenerator::~LWBranchGenerator() {
}

uint64_t LWBranchGenerator::GenerateBranches(const proto::Branch &init,
    proto::OperationType type, const std::string &key, const SpecStore &store,
    std::vector<proto::Branch> &new_branches) {
  proto::Branch opt(init);
  
  const proto::Transaction *mrc;
  
  std::vector<proto::Transaction> seq;
  if (MostRecentConflict(opt.txn().ops(opt.txn().ops_size() - 1), store, seq, mrc)) {
    if (opt.deps().find(mrc->id()) == opt.deps().end()) {
      (*opt.mutable_deps())[mrc->id()] = *mrc;
    }
  }
  new_branches.push_back(opt);
  return 0UL;
}

} /* mortystore */
