#include "store/indicusstore/phase1validator.h"

#include "lib/message.h"
#include "store/indicusstore/common.h"

namespace indicusstore {

Phase1Validator::Phase1Validator(int group, const proto::Transaction *txn,
    const std::string *txnDigest, const transport::Configuration *config,
    KeyManager *keyManager, Parameters params, Verifier *verifier) : group(group),
    txn(txn), txnDigest(txnDigest), config(config), keyManager(keyManager),
    params(params), verifier(verifier), state(NOT_ENOUGH), commits(0U), abstains(0U) {
}

Phase1Validator::~Phase1Validator() {
}

// bool Phase1Validator::ProcessFBMessage(const proto::Phase1FBReply &p1fb){
//
// }

//extend this function to account for p2 replies --> f+1 can serve as proof.
bool Phase1Validator::ProcessMessage(const proto::ConcurrencyControl &cc, bool failureActive) {
  if (params.validateProofs && cc.txn_digest() != *txnDigest) {
    Debug("[group %d] Phase1Reply digest %s does not match computed digest %s.",
        group, BytesToHex(cc.txn_digest(), 16).c_str(),
        BytesToHex(*txnDigest, 16).c_str());

    std::cerr << "         txnDigest of CC message does not match." << std::endl;
    return false;
  }

  if (state == FAST_COMMIT || state == FAST_ABORT) {
    Debug("[group %d] Processing Phas1Reply after already confirmed COMMIT or"
        " ABORT.", group);
    return false;
  }

  if (failureActive && params.injectFailure.type == InjectFailureType::CLIENT_EQUIVOCATE) {
    return EquivocateVotes(cc);
  }

  switch(cc.ccr()) {

    case proto::ConcurrencyControl::ABORT: {
      if(!cc.committed_conflict().has_txn()){
          Panic("Process P1 Validator CONFLICT TXN for Aborted txn: %s", BytesToHex(*txnDigest,64).c_str());
      }

      std::string committedTxnDigest = TransactionDigest(
          cc.committed_conflict().txn(), params.hashDigest);
      //TODO: RECOMMENT, just testing
      if (params.validateProofs && !ValidateCommittedConflict(cc.committed_conflict(),
            &committedTxnDigest, txn, txnDigest, params.signedMessages,
            keyManager, config, verifier)) {
        Debug("[group %d] Invalid committed_conflict for Phase1Reply.",
            group);
        std::cerr << "         Proof verification fails" << std::endl;

        // for(const ReadMessage& read : txn->read_set()){
        //   std::cerr<< "dependent txn has read key: " << read.key() << std::endl;
        // }
        //
        // for(const WriteMessage& write : txn->write_set()){
        //   std::cerr<< "dependent txn has write key: " << write.key() << std::endl;
        // }
        //
        // for(const ReadMessage& read : cc.committed_conflict().txn().read_set()){
        //   std::cerr<< "conflict txn has read key: " << read.key() << std::endl;
        // }
        //
        // for(const WriteMessage& write : cc.committed_conflict().txn().write_set()){
        //   std::cerr<< "conflict txn has write key: " << write.key() << std::endl;
        // }

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
      } else if (commits >= SlowCommitQuorumSize(config) && abstains == 0) { //Could still go FP
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


      if (abstains >= FastAbortQuorumSize(config)){
        state = FAST_ABSTAIN;
      }

      else if (state == SLOW_COMMIT_TENTATIVE) { //can no longer go commit FP
        state = SLOW_COMMIT_FINAL;
      } else if (abstains >= SlowAbortQuorumSize(config) &&
          config->n - abstains  >= SlowCommitQuorumSize(config)) {  //could still go commit slowpath
        Debug("[group %d] Tentative SLOW_ABORT after processing %u COMMIT"
            " replies and %u ABSTAIN replies.", group, commits, abstains);
        state = SLOW_ABORT_TENTATIVE;
      } else if (abstains >= SlowAbortQuorumSize(config)) { // if received 2f+1 aborts will go sp abort immediately, but really we would like to still go fast path abort
        Debug("[group %d] Final SLOW_ABORT_TENTATIVE after processing %u COMMIT"
            " replies and %u ABSTAIN replies.", group, commits, abstains);
        state = SLOW_ABORT_TENTATIVE2;
      }
      else if(abstains >= SlowAbortQuorumSize(config) &&
          config->n - commits < FastAbortQuorumSize(config)){ //not enough commits left for Abort fp, go slow always.
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

bool Phase1Validator::EquivocateVotes(const proto::ConcurrencyControl &cc) {
  switch(cc.ccr()) {

    case proto::ConcurrencyControl::ABORT: {
      // when there's a committed conflict, there's no hope for equivocation
      std::string committedTxnDigest = TransactionDigest(
          cc.committed_conflict().txn(), params.hashDigest);
      //TODO: RECOMMENT, just testing
      if (params.validateProofs && !ValidateCommittedConflict(cc.committed_conflict(),
            &committedTxnDigest, txn, txnDigest, params.signedMessages,
            keyManager, config, verifier)) {
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
        // TODO switch order
      } else if (commits >= SlowCommitQuorumSize(config) && abstains == 0 && !EquivocationPossible()) { //Could still go FP
        Debug("[group %d] Tentative SLOW_COMMIT after processing %u COMMIT"
            " replies and %u ABSTAIN replies.", group, commits, abstains);
        state = SLOW_COMMIT_TENTATIVE;
      } else if (EquivocationReady()) {
        Debug("[group %d] Attempt EQUIVOCATION after processing %u COMMIT"
            " replies and %u ABSTAIN replies.", group, commits, abstains);
        state = EQUIVOCATE;
      } else if (commits >= SlowCommitQuorumSize(config) && !EquivocationPossible()) {
        Debug("[group %d] Final SLOW_COMMIT after processing %u COMMIT"
            " replies and %u ABSTAIN replies, with no hope of equivocation.", group, commits, abstains);
        state = SLOW_COMMIT_FINAL;
      }
      break;

    case proto::ConcurrencyControl::ABSTAIN:
      abstains++;

      if (abstains >= FastAbortQuorumSize(config)){
        state = FAST_ABSTAIN;
      } else if (EquivocationReady()) {
        Debug("[group %d] Attempt EQUIVOCATION after processing %u COMMIT"
            " replies and %u ABSTAIN replies.", group, commits, abstains);
        state = EQUIVOCATE;
      } else if (state == SLOW_COMMIT_TENTATIVE && !EquivocationPossible()) { //can no longer go commit FP
        state = SLOW_COMMIT_FINAL;
      } else if (abstains >= SlowAbortQuorumSize(config) &&
          config->n - abstains  >= SlowCommitQuorumSize(config) &&
          !EquivocationPossible()) {  //could still go commit slowpath
        Debug("[group %d] Tentative SLOW_ABORT after processing %u COMMIT"
            " replies and %u ABSTAIN replies.", group, commits, abstains);
        state = SLOW_ABORT_TENTATIVE;
      } else if (abstains >= SlowAbortQuorumSize(config) && !EquivocationPossible()) { // if received 2f+1 aborts will go sp abort immediately, but really we would like to still go fast path abort
        Debug("[group %d] Final SLOW_ABORT_TENTATIVE after processing %u COMMIT"
            " replies and %u ABSTAIN replies.", group, commits, abstains);
        state = SLOW_ABORT_TENTATIVE2;
      } else if(abstains >= SlowAbortQuorumSize(config) &&
          config->n - commits < FastAbortQuorumSize(config) &&
          !EquivocationPossible()) { //not enough commits left for Abort fp, go slow always.
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
