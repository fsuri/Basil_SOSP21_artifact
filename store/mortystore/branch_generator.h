#ifndef BRANCH_GENERATOR_H
#define BRANCH_GENERATOR_H

#include "lib/latency.h"
#include "store/mortystore/common.h"
#include "store/mortystore/morty-proto.pb.h"
#include "store/mortystore/specstore.h"

#include <string>
#include <vector>

namespace mortystore {

typedef std::unordered_map<uint64_t,
    std::unordered_set<proto::Branch, BranchHasher, BranchComparer>> BranchMap;
typedef std::unordered_set<proto::Branch, BranchHasher, BranchComparer> BranchSet; 

class BranchGenerator {
 public:
  BranchGenerator();
  virtual ~BranchGenerator();

  uint64_t GenerateBranches(const proto::Branch &init, proto::OperationType type,
      const std::string &key, const SpecStore &store,
      std::vector<proto::Branch> &new_branches);

  void AddPending(const proto::Branch &branch);
  void ClearPending(uint64_t txn_id);
 private:
  void AddPendingRead(const std::string &key, const proto::Branch &branch);
  void AddPendingWrite(const std::string &key, const proto::Branch &branch);
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

  std::unordered_map<std::string, BranchMap> pending_writes;
  std::unordered_map<std::string, BranchMap> pending_reads;

  BranchSet already_generated;
  Latency_t generateLatency;
};

} /* mortystore */

#endif /* BRANCH_GENERATOR_H */
