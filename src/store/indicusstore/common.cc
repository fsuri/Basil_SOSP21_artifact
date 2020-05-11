#include "store/indicusstore/common.h"

#include <sstream>

#include <cryptopp/sha.h>
#include <cryptopp/blake2.h>

#include "store/common/timestamp.h"
#include "store/common/transaction.h"

namespace indicusstore {

void SignMessage(const ::google::protobuf::Message &msg,
    crypto::PrivKey* privateKey, uint64_t processId,
    proto::SignedMessage *signedMessage) {
  signedMessage->set_process_id(processId);
  UW_ASSERT(msg.SerializeToString(signedMessage->mutable_data()));
  Debug("Signing data %s with priv key %s.",
      BytesToHex(signedMessage->data(), 128).c_str(),
      BytesToHex(std::string(reinterpret_cast<const char*>(privateKey), 64), 128).c_str());
  *signedMessage->mutable_signature() = crypto::Sign(privateKey,
      signedMessage->data());
}

bool ValidateCommittedConflict(const proto::CommittedProof &proof,
    const std::string *committedTxnDigest, const proto::Transaction *txn,
    const std::string *txnDigest, bool signedMessages, KeyManager *keyManager,
    const transport::Configuration *config) {
  if (signedMessages && !ValidateCommittedProof(proof, committedTxnDigest,
        keyManager, config)) {
    return false;
  }

  if (!TransactionsConflict(proof.txn(), *txn)) {
    Debug("Committed txn [%lu:%lu][%s] does not conflict with this txn [%lu:%lu][%s].",
        proof.txn().client_id(), proof.txn().client_seq_num(),
        BytesToHex(*committedTxnDigest, 16).c_str(),
        txn->client_id(), txn->client_seq_num(),
        BytesToHex(*txnDigest, 16).c_str());
    return false;
  }

  return true;
}

bool ValidateCommittedProof(const proto::CommittedProof &proof,
    const std::string *committedTxnDigest, KeyManager *keyManager,
    const transport::Configuration *config) {
  if (proof.txn().client_id() == 0UL && proof.txn().client_seq_num() == 0UL) {
    // TODO: this is unsafe, but a hack so that we can bootstrap a benchmark
    //    without needing to write all existing data with transactions
    return true;
  }

  if (proof.has_p1_sigs()) {
    return ValidateP1Replies(proto::COMMIT, true, &proof.txn(), committedTxnDigest,
        proof.p1_sigs(), keyManager, config, -1, proto::ConcurrencyControl::ABORT);
  } else if (proof.has_p2_sigs()) {
    return ValidateP2Replies(proto::COMMIT, committedTxnDigest, proof.p2_sigs(),
        keyManager, config, -1, proto::ABORT);
  } else {
    Debug("Proof has neither P1 nor P2 sigs.");
    return false;
  }
}

bool ValidateP1Replies(proto::CommitDecision decision,
    bool fast,
    const proto::Transaction *txn,
    const std::string *txnDigest,
    const proto::GroupedSignatures &groupedSigs,
    KeyManager *keyManager,
    const transport::Configuration *config,
    int64_t myProcessId, proto::ConcurrencyControl::Result myResult) {
  Latency_t dummyLat;
  //_Latency_Init(&dummyLat, "dummy_lat");
  return ValidateP1Replies(decision, fast, txn, txnDigest, groupedSigs,
      keyManager, config, myProcessId, myResult, dummyLat);
}

bool ValidateP1Replies(proto::CommitDecision decision,
    bool fast,
    const proto::Transaction *txn,
    const std::string *txnDigest,
    const proto::GroupedSignatures &groupedSigs,
    KeyManager *keyManager,
    const transport::Configuration *config,
    int64_t myProcessId, proto::ConcurrencyControl::Result myResult,
    Latency_t &lat) {
  proto::ConcurrencyControl concurrencyControl;
  concurrencyControl.Clear();
  *concurrencyControl.mutable_txn_digest() = *txnDigest;
  uint32_t quorumSize = 0;
  if (fast && decision == proto::COMMIT) {
    concurrencyControl.set_ccr(proto::ConcurrencyControl::COMMIT);
    quorumSize = config->n;
  } else if (decision == proto::COMMIT) {
    concurrencyControl.set_ccr(proto::ConcurrencyControl::COMMIT);
    quorumSize = SlowCommitQuorumSize(config);
  } else if (decision == proto::ABORT) {
    concurrencyControl.set_ccr(proto::ConcurrencyControl::ABSTAIN);
    quorumSize = SlowAbortQuorumSize(config);
  } else {
    // NOT_REACHABLE();
    return false;
  }

  std::string ccMsg;
  concurrencyControl.SerializeToString(&ccMsg);
  
  std::set<int> groupsVerified;
  std::set<uint64_t> replicasVerified;
  for (const auto &sigs : groupedSigs.grouped_sigs()) {
    uint32_t verified = 0;
    for (const auto &sig : sigs.second.sigs()) {

      if (!IsReplicaInGroup(sig.process_id(), sigs.first, config)) {
        Debug("Signature for group %lu from replica %lu who is not in group.",
            sigs.first, sig.process_id());
        return false;
      }

      bool skip = false;
      if (sig.process_id() == myProcessId && myProcessId >= 0) {
        if (concurrencyControl.ccr() == myResult) {
          skip = true;
        } else {
          Debug("Signature purportedly from replica %lu"
              " (= my id %ld) doesn't match my response %u.",
              sig.process_id(), myProcessId, concurrencyControl.ccr());
          return false;
        }
      }

      Debug("Verifying %lu byte signature from replica %lu in group %lu.",
          sig.signature().size(), sig.process_id(), sigs.first);
      //Latency_Start(&lat);
      if (!skip && !crypto::Verify(keyManager->GetPublicKey(sig.process_id()), ccMsg,
              sig.signature())) {
        //Latency_End(&lat);
        Debug("Signature from replica %lu in group %lu is not valid.",
            sig.process_id(), sigs.first);
        return false;
      }
      //Latency_End(&lat);
      //
      auto insertItr = replicasVerified.insert(sig.process_id());
      if (!insertItr.second) {
        Debug("Already verified sig from replica %lu in group %lu.",
            sig.process_id(), sigs.first);
        return false;
      }
      verified++;
    }

    if (verified != quorumSize) {
      Debug("Expected exactly %u sigs but processed %u.", quorumSize, verified);
      return false;
    }

    groupsVerified.insert(sigs.first);
  }

  if (decision == proto::COMMIT) {
    for (auto group : txn->involved_groups()) {
      if (groupsVerified.find(group) == groupsVerified.end()) {
        Debug("No Phase1Replies for involved_group %ld.", group);
        return false;
      }
    }
  }

  return true;
}

bool ValidateP2Replies(proto::CommitDecision decision,
    const std::string *txnDigest, const proto::GroupedSignatures &groupedSigs,
    KeyManager *keyManager, const transport::Configuration *config,
    int64_t myProcessId, proto::CommitDecision myDecision) {
  Latency_t dummyLat;
  //_Latency_Init(&dummyLat, "dummy_lat");
  return ValidateP2Replies(decision, txnDigest, groupedSigs,
      keyManager, config, myProcessId, myDecision, dummyLat);
}

bool ValidateP2Replies(proto::CommitDecision decision,
    const std::string *txnDigest, const proto::GroupedSignatures &groupedSigs,
    KeyManager *keyManager, const transport::Configuration *config,
    int64_t myProcessId, proto::CommitDecision myDecision,
    Latency_t &lat) {
  proto::Phase2Decision p2Decision;
  p2Decision.Clear();
  p2Decision.set_decision(decision);
  *p2Decision.mutable_txn_digest() = *txnDigest;

  std::string p2DecisionMsg;
  p2Decision.SerializeToString(&p2DecisionMsg);

  if (groupedSigs.grouped_sigs().size() != 1) {
    Debug("Expected exactly 1 group but saw %lu", groupedSigs.grouped_sigs().size());
    return false;
  }
  
  const auto &sigs = groupedSigs.grouped_sigs().begin();
  uint32_t verified = 0;
  std::set<uint64_t> replicasVerified;
  for (const auto &sig : sigs->second.sigs()) {
    //Latency_Start(&lat);

    bool skip = false;
    if (sig.process_id() == myProcessId && myProcessId >= 0) {
      if (p2Decision.decision() == myDecision) {
        skip = true;
      } else {
        return false;
      }
    }

    if (!skip && !crypto::Verify(keyManager->GetPublicKey(sig.process_id()),
          p2DecisionMsg, sig.signature())) {
      //Latency_End(&lat);
      Debug("Signature from %lu is not valid.", sig.process_id());
      return false;
    }
    //Latency_End(&lat);

    if (!replicasVerified.insert(sig.process_id()).second) {
      return false;
    }
    verified++;
  }

  if (verified != QuorumSize(config)) {
    Debug("Expected exactly %lu sigs but processed %u.", QuorumSize(config),
        verified);
    return false;
  }

  return true;
}

bool ValidateTransactionWrite(const proto::CommittedProof &proof,
    const std::string *txnDigest,
    const std::string &key, const std::string &val, const Timestamp &timestamp,
    const transport::Configuration *config, bool signedMessages,
    KeyManager *keyManager) {
  if (proof.txn().client_id() == 0UL && proof.txn().client_seq_num() == 0UL) {
    // TODO: this is unsafe, but a hack so that we can bootstrap a benchmark
    //    without needing to write all existing data with transactions
    return true;
  }

  if (signedMessages && !ValidateCommittedProof(proof, txnDigest,
        keyManager, config)) {
    Debug("VALIDATE CommittedProof failed for txn %lu.%lu.",
        proof.txn().client_id(), proof.txn().client_seq_num());
    return false;
  }

  if (Timestamp(proof.txn().timestamp()) != timestamp) {
    Debug("VALIDATE timestamp failed for txn %lu.%lu: txn ts %lu.%lu != returned"
        " ts %lu.%lu.", proof.txn().client_id(), proof.txn().client_seq_num(),
        proof.txn().timestamp().timestamp(), proof.txn().timestamp().id(),
        timestamp.getTimestamp(), timestamp.getID());
    return false;
  }

  bool keyInWriteSet = false;
  for (const auto &write : proof.txn().write_set()) {
    if (write.key() == key) {
      keyInWriteSet = true;
      if (write.value() != val) {
        Debug("VALIDATE value failed for txn %lu.%lu key %s: txn value %s != "
            "returned value %s.", proof.txn().client_id(),
            proof.txn().client_seq_num(), BytesToHex(key, 16).c_str(),
            BytesToHex(write.value(), 16).c_str(), BytesToHex(val, 16).c_str());
        return false;
      }
      break;
    }
  }

  if (!keyInWriteSet) {
    Debug("VALIDATE value failed for txn %lu.%lu; key %s not written.",
        proof.txn().client_id(), proof.txn().client_seq_num(),
        BytesToHex(key, 16).c_str());
    return false;
  }

  return true;
}

bool ValidateDependency(const proto::Dependency &dep,
    const transport::Configuration *config, uint64_t readDepSize,
    KeyManager *keyManager) {
  if (dep.write_sigs().sigs_size() < readDepSize) {
    return false;
  }

  std::string preparedData;
  dep.write().SerializeToString(&preparedData);
  for (const auto &sig : dep.write_sigs().sigs()) {
    if (!crypto::Verify(keyManager->GetPublicKey(sig.process_id()), preparedData,
          sig.signature())) {
      return false;    
    }
  }
  return true;
}

bool operator==(const proto::Write &pw1, const proto::Write &pw2) {
  return pw1.committed_value() == pw2.committed_value() &&
    pw1.committed_timestamp().timestamp() == pw2.committed_timestamp().timestamp() &&
    pw1.committed_timestamp().id() == pw2.committed_timestamp().id() &&
    pw1.prepared_value() == pw2.prepared_value() &&
    pw1.prepared_timestamp().timestamp() == pw2.prepared_timestamp().timestamp() &&
    pw1.prepared_timestamp().id() == pw2.prepared_timestamp().id() &&
    pw1.prepared_txn_digest() == pw2.prepared_txn_digest();
}

bool operator!=(const proto::Write &pw1, const proto::Write &pw2) {
  return !(pw1 == pw2);
}

std::string TransactionDigest(const proto::Transaction &txn, bool hashDigest) {
  if (hashDigest) {
    CryptoPP::BLAKE2b hash;
    std::string digest;

    uint64_t client_id = txn.client_id();
    uint64_t client_seq_num = txn.client_seq_num();
    hash.Update((const CryptoPP::byte*) &client_id, sizeof(client_id));
    hash.Update((const CryptoPP::byte*) &client_seq_num, sizeof(client_seq_num));
    for (const auto &group : txn.involved_groups()) {
      hash.Update((const CryptoPP::byte*) &group, sizeof(group));
    }
    for (const auto &read : txn.read_set()) {
      uint64_t readtimeId = read.readtime().id();
      uint64_t readtimeTs = read.readtime().timestamp();
      hash.Update((const CryptoPP::byte*) &read.key()[0], read.key().length());
      hash.Update((const CryptoPP::byte*) &readtimeId,
          sizeof(read.readtime().id()));
      hash.Update((const CryptoPP::byte*) &readtimeTs,
          sizeof(read.readtime().timestamp()));
    }
    for (const auto &write : txn.write_set()) {
      hash.Update((const CryptoPP::byte*) &write.key()[0], write.key().length());
      hash.Update((const CryptoPP::byte*) &write.value()[0], write.value().length());
    }
    for (const auto &dep : txn.deps()) {
      hash.Update((const CryptoPP::byte*) &dep.write().prepared_txn_digest()[0],
          dep.write().prepared_txn_digest().length());
    }
    uint64_t timestampId = txn.timestamp().id();
    uint64_t timestampTs = txn.timestamp().timestamp();
    hash.Update((const CryptoPP::byte*) &timestampId,
        sizeof(timestampId));
    hash.Update((const CryptoPP::byte*) &timestampTs,
        sizeof(timestampTs));
    
    digest.resize(hash.DigestSize());
    hash.Final((CryptoPP::byte*) &digest[0]);

    return digest;
  } else {
    char digestChar[16];
    *reinterpret_cast<uint64_t *>(digestChar) = txn.client_id();
    *reinterpret_cast<uint64_t *>(digestChar + 8) = txn.client_seq_num();
    return std::string(digestChar, 16);
  }
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

uint64_t QuorumSize(const transport::Configuration *config) {
  return 4 * static_cast<uint64_t>(config->f) + 1;
}

uint64_t FastQuorumSize(const transport::Configuration *config) {
  return static_cast<uint64_t>(config->n);
}

uint64_t SlowCommitQuorumSize(const transport::Configuration *config) {
  return 3 * static_cast<uint64_t>(config->f) + 1;
}

uint64_t SlowAbortQuorumSize(const transport::Configuration *config) {
  return static_cast<uint64_t>(config->f) + 1;
}

bool IsReplicaInGroup(uint64_t id, uint32_t group,
    const transport::Configuration *config) {
  return id / config->n == group;
}

} // namespace indicusstore
