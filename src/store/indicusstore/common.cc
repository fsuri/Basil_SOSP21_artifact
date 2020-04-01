#include "store/indicusstore/common.h"

#include <cryptopp/sha.h>

#include "store/common/timestamp.h"
#include "store/common/transaction.h"

namespace indicusstore {

bool ValidateSignedMessage(const proto::SignedMessage &signedMessage,
    KeyManager *keyManager, ::google::protobuf::Message &plaintextMsg) {
  proto::PackedMessage packedMessage;
  if (!__PreValidateSignedMessage(signedMessage, keyManager, packedMessage)) {
    return false;
  }

  if (packedMessage.type() != plaintextMsg.GetTypeName()) {
    return false;
  }

  plaintextMsg.ParseFromString(packedMessage.msg());
  return true;
}

bool ValidateSignedMessage(const proto::SignedMessage &signedMessage,
    KeyManager *keyManager, std::string &data, std::string &type) {
  proto::PackedMessage packedMessage;
  if (!__PreValidateSignedMessage(signedMessage, keyManager, packedMessage)) {
    return false;
  }

  data = packedMessage.msg();
  type = packedMessage.type();
  return true;
}

bool __PreValidateSignedMessage(const proto::SignedMessage &signedMessage,
    KeyManager *keyManager, proto::PackedMessage &packedMessage) {
  crypto::PubKey replicaPublicKey = keyManager->GetPublicKey(
      signedMessage.process_id());
  // verify that the replica actually sent this reply and that we are expecting
  // this reply
  if (!crypto::IsMessageValid(replicaPublicKey, signedMessage.packed_msg(),
        &signedMessage)) {
    return false;
  }

  return packedMessage.ParseFromString(signedMessage.packed_msg());
}

void SignMessage(const ::google::protobuf::Message &msg,
    const crypto::PrivKey &privateKey, uint64_t processId,
    proto::SignedMessage &signedMessage) {
  proto::PackedMessage packedMsg;
  *packedMsg.mutable_msg() = msg.SerializeAsString();
  *packedMsg.mutable_type() = msg.GetTypeName();
  std::string msgData = packedMsg.SerializeAsString();
  crypto::SignMessage(privateKey, msgData, signedMessage);
  signedMessage.set_packed_msg(msgData);
  signedMessage.set_process_id(processId);
}

proto::CommitDecision IndicusDecide(
    const std::map<int, std::vector<proto::Phase1Reply>> &replies,
    const transport::Configuration *config, bool validateProofs,
    bool signedMessages, KeyManager *keyManager) {
  bool fast;
  for (const auto &groupReplies : replies) {
    proto::CommitDecision groupDecision = IndicusShardDecide(groupReplies.second,
        config, validateProofs, signedMessages, keyManager, fast);
    if (groupDecision == proto::ABORT) {
      return proto::ABORT;
    }
  }
  return proto::COMMIT;
}

proto::CommitDecision IndicusShardDecide(
    const std::vector<proto::Phase1Reply> &replies,
    const transport::Configuration *config, bool validateProofs,
    bool signedMessages, KeyManager *keyManager, bool &fast) {
  int commits = 0;
  int abstains = 0;

  Timestamp ts = 0;
  proto::CommitDecision decision;

  for (const auto& reply : replies) {
    if (reply.ccr() == proto::Phase1Reply::ABORT) {
      if (validateProofs) {
        if (!ValidateProof(reply.committed_conflict(), config, signedMessages,
              keyManager)) {
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

bool ValidateTransactionWrite(const proto::CommittedProof &proof,
    const std::string &key, const std::string &val, const Timestamp &timestamp,
    const transport::Configuration *config, bool signedMessages,
    KeyManager *keyManager) {
  if (!ValidateProof(proof, config, signedMessages, keyManager)) {
    return false;
  }

  if (Timestamp(proof.txn().timestamp()) != timestamp) {
    return false;
  }

  bool keyInWriteSet = false;
  for (const auto &write : proof.txn().write_set()) {
    if (write.key() == key) {
      keyInWriteSet = true;
      if (write.value() != val) {
        return false;
      }
    }
  }
  
  if (!keyInWriteSet) {
    return false;
  }

  return true;
}

bool ValidateProof(const proto::CommittedProof &proof,
    const transport::Configuration *config, bool signedMessages,
    KeyManager *keyManager) {
  std::string txnDigest = TransactionDigest(proof.txn());
  if (signedMessages) {
    if (proof.has_signed_p1_replies()) {
      std::map<int, std::vector<proto::Phase1Reply>> groupedP1Replies;
      for (const auto &signedP1Replies : proof.signed_p1_replies().replies()) {
        std::vector<proto::Phase1Reply> p1Replies;
        for (const auto &signedP1Reply : signedP1Replies.second.msgs()) {
          proto::Phase1Reply reply;
          if (!ValidateSignedMessage(signedP1Reply, keyManager, reply)) {
            return false;
          }
          
          p1Replies.push_back(reply);
        }
        groupedP1Replies.insert(std::make_pair(signedP1Replies.first, p1Replies));
      }
      return ValidateP1RepliesCommit(groupedP1Replies, txnDigest, proof.txn(),
          config);
    } else if (proof.has_signed_p2_replies()) {
      std::vector<proto::Phase2Reply> p2Replies;
      for (const auto &signedP2Reply : proof.signed_p2_replies().msgs()) {
        proto::Phase2Reply reply;
        if (!ValidateSignedMessage(signedP2Reply, keyManager, reply)) {
          return false;
        }
          
        p2Replies.push_back(reply);
      }   
      return ValidateP2RepliesCommit(p2Replies, txnDigest, proof.txn(), config);
    } else {
      return false;
    }
  } else {
    if (proof.has_p1_replies()) {
      std::map<int, std::vector<proto::Phase1Reply>> groupedP1Replies;
      for (const auto &groupP1Replies : proof.p1_replies().replies()) {
        std::vector<proto::Phase1Reply> p1Replies;
        for (const auto &p1Reply : groupP1Replies.second.replies()) {
          p1Replies.push_back(p1Reply);
        }
        groupedP1Replies.insert(std::make_pair(groupP1Replies.first, p1Replies));
      }
      return ValidateP1RepliesCommit(groupedP1Replies, txnDigest, proof.txn(),
          config);
    } else if (proof.has_p2_replies()) {
      std::vector<proto::Phase2Reply> p2Replies;
      for (const auto &p2Reply : proof.p2_replies().replies()) {
        p2Replies.push_back(p2Reply);
      }
      return ValidateP2RepliesCommit(p2Replies, txnDigest, proof.txn(), config);
    } else {
      return false;
    }
  }
}

// Assume that digest has been computed from txn
bool ValidateP1RepliesCommit(
    const std::map<int, std::vector<proto::Phase1Reply>> &groupedP1Replies,
    const std::string &txnDigest, const proto::Transaction &txn,
    const transport::Configuration *config) {
  // TODO: technically only need a single Phase1Reply that says commit ->
  //    messages will all be the same
  for (auto group : txn.involved_groups()) {
    auto repliesItr = groupedP1Replies.find(group);
    if (repliesItr == groupedP1Replies.end()) {
      // missing P1 replies from involved group
      return false;
    }

    if (repliesItr->second.size() < config->n) {
      return false;
    }

    for (const auto &p1Reply : repliesItr->second) {
      if (p1Reply.ccr() != proto::Phase1Reply::COMMIT) {
        return false;
      }
      
      if (p1Reply.txn_digest() != txnDigest) {
        // P1 reply is for different transaction
        return false;
      }
    }
  }
  return true;
}

// Assume that digest has been computed from txn
bool ValidateP2RepliesCommit(
    const std::vector<proto::Phase2Reply> &p2Replies,
    const std::string &txnDigest, const proto::Transaction &txn,
    const transport::Configuration *config) {
  if (p2Replies.size() <= 4 * config->f + 1) {
    return false;
  }

  for (const auto &p2Reply : p2Replies) {
    if (p2Reply.decision() != proto::COMMIT) {
      return false;
    }
  }
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
