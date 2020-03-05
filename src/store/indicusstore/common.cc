#include "store/indicusstore/common.h"

#include "store/common/timestamp.h"
#include "store/common/transaction.h"

namespace indicusstore {

bool ValidateSignedMessage(const proto::SignedMessage &signedMessage,
    const bft_tapir::NodeConfig *cryptoConfig) {
  crypto::PubKey replicaPublicKey = cryptoConfig->getReplicaPublicKey(
      signedMessage.process_id());
  // verify that the replica actually sent this reply and that we are expecting
  // this reply
  return crypto::IsMessageValid(replicaPublicKey, signedMessage.msg(),
        &signedMessage);
}

proto::TxnAction IndicusDecide(const std::vector<proto::Prepare1Reply> &replies,
    const transport::Configuration *config) {
  // If a majority say prepare_ok,
  int ok_count = 0;
  Timestamp ts = 0;
  proto::TxnAction decision;
  proto::Prepare1Reply final_reply;

  for (const auto& reply : replies) {
    if (reply.status() == REPLY_OK) {
      ok_count++;
    } else if (reply.status() == REPLY_FAIL) {
      return proto::TxnAction::ABORT;
    } else if (reply.status() == REPLY_RETRY) {
      Timestamp t(reply.timestamp());
      if (t > ts) {
        ts = t;
      }
    }
  }

  if (ok_count >= config->QuorumSize()) {
    decision = proto::TxnAction::COMMIT;
  } else {
    decision = proto::TxnAction::ABORT;
  }
  return decision;
}

} // namespace indicusstore
