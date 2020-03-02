#ifndef LW_BRANCH_GENERATOR_H
#define LW_BRANCH_GENERATOR_H

#include "lib/latency.h"
#include "store/mortystore/common.h"
#include "store/mortystore/morty-proto.pb.h"
#include "store/mortystore/specstore.h"
#include "store/mortystore/branch_generator.h"

#include <string>
#include <vector>

namespace mortystore {

class LWBranchGenerator : public BranchGenerator {
 public:
  LWBranchGenerator();
  virtual ~LWBranchGenerator();

  virtual uint64_t GenerateBranches(const proto::Branch &init, proto::OperationType type,
      const std::string &key, const SpecStore &store,
      std::vector<proto::Branch> &new_branches) override;

 private:
  BranchSet already_generated;

};

} /* mortystore */

#endif /* LW_BRANCH_GENERATOR_H */
