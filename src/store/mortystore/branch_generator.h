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

  virtual uint64_t GenerateBranches(const proto::Branch &init, proto::OperationType type,
      const std::string &key, const SpecStore &store,
      std::vector<proto::Branch> &new_branches) = 0;

  void AddActive(const proto::Branch &branch);
  void ClearActive(uint64_t txn_id);
 protected:
  inline const std::unordered_map<std::string, BranchMap> &GetActiveWrites() const { return active_writes; }
  inline const std::unordered_map<std::string, BranchMap> &GetActiveReads() const { return active_reads; }
  inline const std::set<uint64_t> GetActiveTids() const { return active_tids; }

 private:
  void AddActiveRead(const std::string &key, const proto::Branch &branch);
  void AddActiveWrite(const std::string &key, const proto::Branch &branch);

  std::set<uint64_t> active_tids;
  std::unordered_map<std::string, BranchMap> active_writes;
  std::unordered_map<std::string, BranchMap> active_reads;
};

} /* mortystore */

#endif /* BRANCH_GENERATOR_H */
