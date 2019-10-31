#ifndef BRANCH_GENERATOR_H
#define BRANCH_GENERATOR_H

#include "store/mortystore/common.h"
#include "store/mortystore/morty-proto.pb.h"

#include <string>
#include <vector>

namespace mortystore {

class BranchGenerator {
 public:
  BranchGenerator();
  virtual ~BranchGenerator();

  void GenerateBranches(const proto::Branch &init, proto::OperationType type,
      const std::string &key, const std::vector<proto::Transaction> &committed,
      std::vector<proto::Branch> &new_branches);

  void AddPendingRead(const std::string &key, const proto::Branch &branch);
  void AddPendingWrite(const std::string &key, const proto::Branch &branch);
  void ClearPending(uint64_t txn_id);
 private:
  void GenerateBranchesSubsets(const std::unordered_map<uint64_t, std::unordered_set<proto::Branch, BranchHasher, BranchComparer>> &pending_branches,
      const std::vector<uint64_t> &txns,
      const std::vector<proto::Transaction> &committed,
      std::vector<proto::Branch> &new_branches,
      std::vector<uint64_t> subset = std::vector<uint64_t>(), int64_t i = -1);
  void GenerateBranchesPermutations(const std::unordered_map<uint64_t, std::unordered_set<proto::Branch, BranchHasher, BranchComparer>> &pending_branches,
      const std::vector<uint64_t> &subset,
      const std::vector<proto::Transaction> &committed,
      std::vector<proto::Branch> &new_branches);

  std::unordered_map<std::string, std::vector<proto::Branch>> pending_reads;
  std::unordered_map<std::string, std::vector<proto::Branch>> pending_writes;
  std::vector<proto::Branch> already_generated;
};

} /* mortystore */

#endif /* BRANCH_GENERATOR_H */
