/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fsp@cs.cornell.edu>
 *                Matthew Burke <matthelb@cs.cornell.edu>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************************/
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

} // namespace indicusstore
