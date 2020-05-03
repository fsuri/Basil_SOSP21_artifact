#ifndef PHASE1_VALIDATOR_H
#define PHASE1_VALIDATOR_H

#include <string>
#include <vector>

#include "lib/configuration.h"
#include "lib/keymanager.h"
#include "lib/transport.h"
#include "store/indicusstore/common.h"
#include "store/indicusstore/indicus-proto.pb.h"

namespace indicusstore {

enum Phase1ValidationState {
  FAST_COMMIT = 0,
  SLOW_COMMIT_TENTATIVE,
  SLOW_COMMIT_FINAL,
  FAST_ABORT,
  SLOW_ABORT_TENTATIVE,
  SLOW_ABORT_FINAL,
  NOT_ENOUGH
};

class Phase1Validator {
 public:
  Phase1Validator(const proto::Transaction *txn, const std::string *txnDigest,
      const transport::Configuration *config, KeyManager *keyManager,
      bool signedMessages, bool hashDigest);
  virtual ~Phase1Validator();

  bool ProcessMessage(int group,
      const proto::Phase1Reply *p1Reply,
      const proto::SignedMessage *signedP1Reply);
  
  inline Phase1ValidationState GetState() const { return state; }
    
 private:
  const proto::Transaction *txn;
  const std::string *txnDigest;
  const transport::Configuration *config;
  KeyManager *keyManager;
  const bool signedMessages;
  const bool hashDigest;

  Phase1ValidationState state;
  uint32_t commits;
  uint32_t abstains;

};

} // namespace indicusstore

#endif /* PHASE1_VALIDATOR_H */
