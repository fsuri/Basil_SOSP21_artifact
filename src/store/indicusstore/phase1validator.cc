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
    Debug("[group %d] Phase1Reply from replica %lu who is not in group.",
        group, signedP1Reply->process_id());
    return false;
  }

  if (p1Reply->txn_digest() != *txnDigest) {
    Debug("[group %d] Phase1Reply digest %s does not match computed digest %s.",
        group, BytesToHex(p1Reply->txn_digest(), 16).c_str(),
        BytesToHex(*txnDigest, 16).c_str());
    return false;
  }

  if (state == FAST_COMMIT || state == FAST_ABORT) {
    Debug("[group %d] Processing Phas1Reply after already confirmed COMMIT or"
        " ABORT.", group);
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
        Debug("[group %d] Invalid committed_conflict for Phase1Reply.",
            group);
        return false;
      }
      break;
    }
    case proto::Phase1Reply::COMMIT:
      commits++;

      if (commits == config->n) {
        Debug("[group %d] FAST_COMMIT after processing %u COMMIT replies.",
            group, commits);
        state = FAST_COMMIT;
      } else if (commits >= SlowCommitQuorumSize(config) && abstains == 0) {
        Debug("[group %d] Tentative SLOW_COMMIT after processing %u COMMIT"
            " replies and %u ABSTAIN replies.", group, commits, abstains);
        state = SLOW_COMMIT_TENTATIVE;
      } else if (commits >= SlowCommitQuorumSize(config)) {
        Debug("[group %d] Final SLOW_COMMIT after processing %u COMMIT"
            " replies and %u ABSTAIN replies.", group, commits, abstains);
        state = SLOW_COMMIT_FINAL;
      }
      break;
    case proto::Phase1Reply::ABSTAIN:
      abstains++;

      if (state == SLOW_COMMIT_TENTATIVE) {
        state = SLOW_COMMIT_FINAL;
      } else if (abstains >= SlowAbortQuorumSize(config) &&
          config->n - abstains  >= SlowCommitQuorumSize(config)) {
        Debug("[group %d] Tentative SLOW_ABORT after processing %u COMMIT"
            " replies and %u ABSTAIN replies.", group, commits, abstains);
        state = SLOW_ABORT_TENTATIVE;
      } else if (abstains >= SlowAbortQuorumSize(config)) {
        Debug("[group %d] Final SLOW_ABORT after processing %u COMMIT"
            " replies and %u ABSTAIN replies.", group, commits, abstains);
        state = SLOW_ABORT_FINAL;
      }
      break;
    default:
      Debug("[group %d] Invalid ccr for Phase1Reply.",
          group);
      return false;
  }

  return true;
}

} // namespace indicusstore
