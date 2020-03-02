#ifndef PERM_BRANCH_GENERATOR_H
#define PERM_BRANCH_GENERATOR_H

#include "lib/latency.h"
#include "store/mortystore/common.h"
#include "store/mortystore/morty-proto.pb.h"
#include "store/mortystore/specstore.h"
#include "store/mortystore/branch_generator.h"

#include <string>
#include <vector>

namespace mortystore {

class PermBranchGenerator : public BranchGenerator {
 public:
  PermBranchGenerator();
  virtual ~PermBranchGenerator();

  virtual uint64_t GenerateBranches(const proto::Branch &init, proto::OperationType type,
      const std::string &key, const SpecStore &store,
      std::vector<proto::Branch> &new_branches) override;

 private:
  void GenerateBranchesSubsets(
      const std::vector<uint64_t> &txns,
      const BranchMap &p_branches,
      const SpecStore &store,
      std::vector<proto::Branch> &new_branches,
      std::vector<uint64_t> subset = std::vector<uint64_t>(), int64_t i = -1);
  void GenerateBranchesPermutations(
      const std::vector<uint64_t> &subset,
      const BranchMap &p_branches,
      const SpecStore &store,
      std::vector<proto::Branch> &new_branches);

  BranchSet already_generated;
  Latency_t generateLatency;
};

} /* mortystore */

#endif /* PERM_BRANCH_GENERATOR_H */
