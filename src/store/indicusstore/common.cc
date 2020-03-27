#include "store/indicusstore/common.h"

#include <cryptopp/sha.h>

#include "store/common/timestamp.h"
#include "store/common/transaction.h"

namespace indicusstore {

bool ValidateSignedMessage(const proto::SignedMessage &signedMessage,
    KeyManager *keyManager) {
  crypto::PubKey replicaPublicKey = keyManager->GetPublicKey(
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

proto::CommitDecision IndicusDecide(
    const std::map<int, std::vector<proto::Phase1Reply>> &replies,
    const transport::Configuration *config, bool validateProofs) {
  bool fast;
  for (const auto &groupReplies : replies) {
    proto::CommitDecision groupDecision = IndicusShardDecide(groupReplies.second,
        config, validateProofs, fast);
    if (groupDecision == proto::ABORT) {
      return proto::ABORT;
    }
  }
  return proto::COMMIT;
}

proto::CommitDecision IndicusShardDecide(
    const std::vector<proto::Phase1Reply> &replies,
    const transport::Configuration *config, bool validateProofs, bool &fast) {
  int commits = 0;
  int abstains = 0;

  Timestamp ts = 0;
  proto::CommitDecision decision;

  for (const auto& reply : replies) {
    if (reply.ccr() == proto::Phase1Reply::ABORT) {
      if (validateProofs) {
        if (!ValidateProof(reply.committed_conflict())) {
          abstains++;
          continue;
        }
      }
      return proto::CommitDecision::ABORT;
    } else if (reply.ccr() == proto::Phase1Reply::ABSTAIN) {
      abstains++;
    } else if (reply.ccr() == proto::Phase1Reply::COMMIT) {
      commits++;
    } // TODO: do we care about RETRY?
  }

  if (commits == config->n) {
    decision = proto::CommitDecision::COMMIT;
    fast = true;
  } else if (abstains >= config->QuorumSize()) { // 3f + 1
    decision = proto::CommitDecision::ABORT;
    fast = true;
  } else if (commits >= config->QuorumSize()) { // 3f + 1
    decision = proto::CommitDecision::COMMIT;
    fast = false;
  } else {
    decision = proto::CommitDecision::ABORT;
    fast = false;
  }
  return decision;
}

bool ValidateCommittedProof(const proto::CommittedProof &proof,
    const std::string &key, const std::string &val, const Timestamp &timestamp) {
  return true;
}

bool ValidateProof(const proto::CommittedProof &proof) {
  return true;
}

std::string TransactionDigest(const proto::Transaction &txn) {
  CryptoPP::SHA256 hash;
  std::string digest;

  uint64_t client_id = txn.client_id();
  uint64_t client_seq_num = txn.client_seq_num();
  hash.Update((const byte*) &client_id, sizeof(client_id));
  hash.Update((const byte*) &client_seq_num, sizeof(client_seq_num));
  for (const auto &group : txn.involved_groups()) {
    hash.Update((const byte*) &group, sizeof(group));
  }
  for (const auto &read : txn.read_set()) {
    uint64_t readtimeId = read.readtime().id();
    uint64_t readtimeTs = read.readtime().timestamp();
    hash.Update((const byte*) &read.key()[0], read.key().length());
    hash.Update((const byte*) &readtimeId,
        sizeof(read.readtime().id()));
    hash.Update((const byte*) &readtimeTs,
        sizeof(read.readtime().timestamp()));
  }
  for (const auto &write : txn.write_set()) {
    hash.Update((const byte*) &write.key()[0], write.key().length());
    hash.Update((const byte*) &write.value()[0], write.value().length());
  }
  for (const auto &dep : txn.deps()) {
    std::string depDigest = TransactionDigest(dep);
    hash.Update((const byte*) &depDigest[0], depDigest.length());
  }
  uint64_t timestampId = txn.timestamp().id();
  uint64_t timestampTs = txn.timestamp().timestamp();
  hash.Update((const byte*) &timestampId,
      sizeof(timestampId));
  hash.Update((const byte*) &timestampTs,
      sizeof(timestampTs));
  
  digest.resize(hash.DigestSize());
  hash.Final((byte*) &digest[0]);

  return digest;
}

} // namespace indicusstore
