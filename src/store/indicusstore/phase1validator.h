#ifndef PHASE1_VALIDATOR_H
#define PHASE1_VALIDATOR_H

#include <string>
#include <vector>

#include "lib/configuration.h"
#include "lib/keymanager.h"
#include "lib/transport.h"
#include "store/indicusstore/common.h"
#include "store/indicusstore/indicus-proto.pb.h"
#include "store/indicusstore/verifier.h"

namespace indicusstore {

enum Phase1ValidationState {
  FAST_COMMIT = 0,
  SLOW_COMMIT_TENTATIVE,
  SLOW_COMMIT_FINAL,
  FAST_ABORT,
  FAST_ABSTAIN,
  SLOW_ABORT_TENTATIVE,
  SLOW_ABORT_TENTATIVE2,
  SLOW_ABORT_FINAL,
  EQUIVOCATE,
  NOT_ENOUGH
};

class Phase1Validator {
 public:
  Phase1Validator(int group, const proto::Transaction *txn,
      const std::string *txnDigest, const transport::Configuration *config,
      KeyManager *keyManager, Parameters params, Verifier *verifier);
  virtual ~Phase1Validator();

  bool ProcessMessage(const proto::ConcurrencyControl &cc);
  bool EquivocateVotes(const proto::ConcurrencyControl &cc);

  inline Phase1ValidationState GetState() const { return state; }
  inline bool EquivocationReady() {
    return commits >= SlowCommitQuorumSize(config) && abstains >= SlowAbortQuorumSize(config);
  }
  bool EquivocationPossible() {
    uint32_t remaining = config->n - commits - abstains;
    uint32_t commits_needed = SlowCommitQuorumSize(config) > commits ? SlowCommitQuorumSize(config) - commits : 0;
    uint32_t abstains_needed = SlowAbortQuorumSize(config) > abstains ? SlowAbortQuorumSize(config) - abstains : 0;
    return remaining >= (commits_needed + abstains_needed);
  }

 private:
  const int group;
  const proto::Transaction *txn;
  const std::string *txnDigest;
  const transport::Configuration *config;
  KeyManager *keyManager;
  const Parameters params;
  Verifier *verifier;

  Phase1ValidationState state;
  uint32_t commits;
  uint32_t abstains;

};

} // namespace indicusstore

#endif /* PHASE1_VALIDATOR_H */
