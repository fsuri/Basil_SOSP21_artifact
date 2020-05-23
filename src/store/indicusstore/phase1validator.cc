#include "store/indicusstore/phase1validator.h"

#include "lib/message.h"
#include "store/indicusstore/common.h"

namespace indicusstore {

Phase1Validator::Phase1Validator(int group, const proto::Transaction *txn,
    const std::string *txnDigest, const transport::Configuration *config,
    KeyManager *keyManager, Parameters params) : group(group),
    txn(txn), txnDigest(txnDigest), config(config), keyManager(keyManager),
    params(params), state(NOT_ENOUGH), commits(0U), abstains(0U) {
}

Phase1Validator::~Phase1Validator() {
}

// bool Phase1Validator::ProcessFBMessage(const proto::Phase1FBReply &p1fb){
//
// }

//extend this function to account for p2 replies --> f+1 can serve as proof.
bool Phase1Validator::ProcessMessage(const proto::ConcurrencyControl &cc) {
  if (params.validateProofs && cc.txn_digest() != *txnDigest) {
    Debug("[group %d] Phase1Reply digest %s does not match computed digest %s.",
        group, BytesToHex(cc.txn_digest(), 16).c_str(),
        BytesToHex(*txnDigest, 16).c_str());
    return false;
  }

  if (state == FAST_COMMIT || state == FAST_ABORT) {
    Debug("[group %d] Processing Phas1Reply after already confirmed COMMIT or"
        " ABORT.", group);
    return false;
  }

  switch(cc.ccr()) {
    case proto::ConcurrencyControl::ABORT: {
      std::string committedTxnDigest = TransactionDigest(
          cc.committed_conflict().txn(), params.hashDigest);
      if (params.validateProofs && !ValidateCommittedConflict(cc.committed_conflict(),
            &committedTxnDigest, txn, txnDigest, params.signedMessages, keyManager, config)) {
        Debug("[group %d] Invalid committed_conflict for Phase1Reply.",
            group);
        return false;
      } else {
        state = FAST_ABORT;
      }
      break;
    }
    case proto::ConcurrencyControl::COMMIT:
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
    case proto::ConcurrencyControl::ABSTAIN:
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
