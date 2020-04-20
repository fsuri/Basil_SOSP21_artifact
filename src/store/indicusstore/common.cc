#include "store/indicusstore/common.h"

#include <sstream>

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
    const proto::Transaction &transaction,
    bool signedMessages, KeyManager *keyManager) {
  bool fast;
  for (const auto &groupReplies : replies) {
    proto::CommitDecision groupDecision = IndicusShardDecide(groupReplies.second,
        config, validateProofs, transaction, signedMessages, keyManager, fast);
    if (groupDecision == proto::ABORT) {
      return proto::ABORT;
    }
  }
  return proto::COMMIT;
}

proto::CommitDecision IndicusShardDecide(
    const std::vector<proto::Phase1Reply> &replies,
    const transport::Configuration *config, bool validateProofs,
    const proto::Transaction &txn,
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

        // TODO: this should be MVTSO conflict, not generic conflict
        if (!TransactionsConflict(txn,
              reply.committed_conflict().txn())) {
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
  } else if (abstains >= 3 * config->f + 1) {
    decision = proto::CommitDecision::ABORT;
    fast = true;
  } else if (commits >= 3 * config->f + 1) {
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
  if (proof.txn().client_id() == 0UL && proof.txn().client_seq_num() == 0UL) {
    // TODO: this is unsafe, but a hack so that we can bootstrap a benchmark
    //    without needing to write all existing data with transactions
    return true;
  }

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
  if (proof.txn().client_id() == 0UL && proof.txn().client_seq_num() == 0UL) {
    // TODO: this is unsafe, but a hack so that we can bootstrap a benchmark
    //    without needing to write all existing data with transactions
    return true;
  }

  std::string txnDigest = TransactionDigest(proof.txn());
  Debug("Validate proof for transaction %lu.%lu (%s).", proof.txn().client_id(),
      proof.txn().client_seq_num(), BytesToHex(txnDigest, 16).c_str());
  if (signedMessages) {
    if (proof.has_signed_p1_replies()) {
      std::map<int, std::vector<proto::Phase1Reply>> groupedP1Replies;
      for (const auto &signedP1Replies : proof.signed_p1_replies().replies()) {
        std::vector<proto::Phase1Reply> p1Replies;
        for (const auto &signedP1Reply : signedP1Replies.second.msgs()) {
          proto::Phase1Reply reply;
          if (!ValidateSignedMessage(signedP1Reply, keyManager, reply)) {
            Debug("Signed P1 reply is not valid.");
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
          Debug("Signed P2 reply is not valid.");
          return false;
        }
          
        p2Replies.push_back(reply);
      }   
      return ValidateP2RepliesCommit(p2Replies, txnDigest, proof.txn(), config);
    } else {
      Debug("Has no signed replies.");
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
      Debug("Has no replies.");
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
      Debug("No P1 replies for group %d", group);
      return false;
    }

    if (repliesItr->second.size() < config->n) {
      Debug("Not enough P1 replies for group %d", group);
      return false;
    }

    for (const auto &p1Reply : repliesItr->second) {
      if (p1Reply.ccr() != proto::Phase1Reply::COMMIT) {
        Debug("Not all COMMIT P1 replies for group %d.", group);
        return false;
      }
      
      if (p1Reply.txn_digest() != txnDigest) {
        // P1 reply is for different transaction
        Debug("P1 reply digest %s does not match this txn digest %s in group %d.",
            BytesToHex(p1Reply.txn_digest(), 16).c_str(), BytesToHex(txnDigest, 16).c_str(),
            group);
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
  if (p2Replies.size() < 4 * config->f + 1) {
    Debug("Not enough P2 replies.");
    return false;
  }

  for (const auto &p2Reply : p2Replies) {
    if (p2Reply.decision() != proto::COMMIT) {
      Debug("Not all P2 COMMIT replies.");
      return false;
    }
    
    if (p2Reply.txn_digest() != txnDigest) {
      Debug("P2 reply digest %s does not match this txn digest %s.",
          BytesToHex(p2Reply.txn_digest(), 16).c_str(),
          BytesToHex(txnDigest, 16).c_str());
      return false;
    }
  }
  return true;
}

bool ValidateDependency(const proto::Dependency &dep,
    const transport::Configuration *config, bool signedMessages,
    KeyManager *keyManager) {
  if (signedMessages) {
    if (dep.proof().has_signed_prepared()) {
      if (dep.proof().signed_prepared().msgs().size() < config->f + 1) {
        return false;
      }

      for (const auto &signedWrite : dep.proof().signed_prepared().msgs()) {
        proto::PreparedWrite write;
        if (!ValidateSignedMessage(signedWrite, keyManager, write)) {
          return false;
        }
        
        if (write != dep.prepared()) {
          return false;
        }
      }
      return true;
    } else {
      return false;
    }
  } else {
    if (dep.proof().prepared().writes().size() < config->f + 1) {
      return false;
    }

    for (const auto &write : dep.proof().prepared().writes()) {
      if (write != dep.prepared()) {
        return false;
      }
    }
    return true;
  }
}

bool operator==(const proto::PreparedWrite &pw1, const proto::PreparedWrite &pw2) {
  return pw1.value() == pw2.value() &&
    pw1.timestamp().timestamp() == pw2.timestamp().timestamp() &&
    pw1.timestamp().id() == pw2.timestamp().id() &&
    pw1.txn_digest() == pw2.txn_digest();
}

bool operator!=(const proto::PreparedWrite &pw1, const proto::PreparedWrite &pw2) {
  return !(pw1 == pw2);
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
    hash.Update((const byte*) &dep.prepared().txn_digest()[0],
        dep.prepared().txn_digest().length());
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

std::string BytesToHex(const std::string &bytes, size_t maxLength) {
  static const char digits[] = "0123456789abcdef";
  std::string hex;
  size_t length = (bytes.size() < maxLength) ? bytes.size() : maxLength;
  for (size_t i = 0; i < length; ++i) {
    hex.push_back(digits[static_cast<uint8_t>(bytes[i]) >> 4]);
    hex.push_back(digits[static_cast<uint8_t>(bytes[i]) & 0xF]);
  }
  return hex;
}

bool TransactionsConflict(const proto::Transaction &a,
    const proto::Transaction &b) {
  for (const auto &ra : a.read_set()) {
    for (const auto &wb : b.write_set()) {
      if (ra.key() == wb.key()) {
        return true;
      }
    }
  }
  for (const auto &rb : b.read_set()) {
    for (const auto &wa : a.write_set()) {
      if (rb.key() == wa.key()) {
        return true;
      }
    }
  }
  for (const auto &wa : a.write_set()) {
    for (const auto &wb : b.write_set()) {
      if (wa.key() == wb.key()) {
        return true;
      }
    }
  }
  return false;
}

} // namespace indicusstore
