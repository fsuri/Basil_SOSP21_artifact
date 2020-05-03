#include "store/indicusstore/phase1validator.h"

#include "lib/message.h"
#include "store/indicusstore/common.h"

namespace indicusstore {

Phase1Validator::Phase1Validator(const proto::Transaction *txn,
    const std::string *txnDigest, const transport::Configuration *config,
    KeyManager *keyManager, bool signedMessages, bool hashDigest) : txn(txn),
    txnDigest(txnDigest), config(config), keyManager(keyManager),
    signedMessages(signedMessages), hashDigest(hashDigest), 
    state(NOT_ENOUGH), commits(0U), abstains(0U) {
}

Phase1Validator::~Phase1Validator() {
}

bool Phase1Validator::ProcessMessage(int group,
    const proto::Phase1Reply *p1Reply,
    const proto::SignedMessage *signedP1Reply) {
  if (signedMessages && !IsReplicaInGroup(group,
        signedP1Reply->process_id(), config)) {
    return false;
  }

  if (p1Reply->txn_digest() != *txnDigest) {
    return false;
  }

  if (state == FAST_COMMIT || state == FAST_ABORT) {
    return false;
  }

  switch(p1Reply->ccr()) {
    case proto::Phase1Reply::ABORT: {
      std::string committedTxnDigest = TransactionDigest(
          p1Reply->committed_conflict().txn(), hashDigest);
      if (ValidateProofCommit(p1Reply->committed_conflict(),
            committedTxnDigest, config, signedMessages, keyManager)) {
        state = FAST_ABORT;
      } else {
        Debug("Invalid committed_conflict for Phase1Reply in group %d.",
            group);
        return false;
      }
      break;
    }
    case proto::Phase1Reply::COMMIT:
      commits++;

      if (commits == config->n) {
        state = FAST_COMMIT;
      } else if (commits >= SlowCommitQuorumSize(config) && abstains == 0) {
        state = SLOW_COMMIT_TENTATIVE;
      } else if (commits >= SlowCommitQuorumSize(config)) {
        state = SLOW_COMMIT_FINAL;
      }
      break;
    case proto::Phase1Reply::ABSTAIN:
      abstains++;

      if (abstains >= SlowAbortQuorumSize(config) &&
          config->n - abstains  >= SlowCommitQuorumSize(config)) {
        state = SLOW_ABORT_TENTATIVE;
      } else if (abstains >= SlowAbortQuorumSize(config)) {
        state = SLOW_ABORT_FINAL;
      }
      break;
    default:
      Debug("Invalid ccr for Phase1Reply in group %d.",
          group);
      return false;
  }

  return true;
}

} // namespace indicusstore
