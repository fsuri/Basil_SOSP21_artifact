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

void SignMessage(const ::google::protobuf::Message &msg,
    const crypto::PrivKey &privateKey, uint64_t processId,
    proto::SignedMessage &signedMessage) {
  std::string msgData = msg.SerializeAsString();
  crypto::SignMessage(privateKey, msgData, signedMessage);
  signedMessage.set_msg(msgData);
  signedMessage.set_type(msg.GetTypeName());
  signedMessage.set_process_id(processId);
}

proto::TxnDecision IndicusDecide(const std::vector<proto::Phase1Reply> &replies,
    const transport::Configuration *config) {
  // If a majority say prepare_ok,
  int ok_count = 0;
  Timestamp ts = 0;
  proto::TxnDecision decision;
  proto::Phase1Reply final_reply;

  for (const auto& reply : replies) {
    if (reply.status() == REPLY_OK) {
      ok_count++;
    } else if (reply.status() == REPLY_FAIL) {
      return proto::TxnDecision::ABORT;
    } else if (reply.status() == REPLY_RETRY) {
      Timestamp t(reply.timestamp());
      if (t > ts) {
        ts = t;
      }
    }
  }

  if (ok_count >= config->QuorumSize()) {
    decision = proto::TxnDecision::COMMIT;
  } else {
    decision = proto::TxnDecision::ABORT;
  }
  return decision;
}

} // namespace indicusstore
