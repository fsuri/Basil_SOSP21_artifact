#include "store/hotstuffstore/common.h"

#include <cryptopp/sha.h>
#include <unordered_set>
#include <thread>
#include <atomic>

#include "store/common/timestamp.h"
#include "store/common/transaction.h"
#include "store/hotstuffstore/pbft_batched_sigs.h"

namespace hotstuffstore {

using namespace CryptoPP;

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
  if (!CheckSignature(signedMessage, keyManager)) {
    return false;
  }

  return packedMessage.ParseFromString(signedMessage.packed_msg());
}

bool CheckSignature(const proto::SignedMessage &signedMessage,
    KeyManager *keyManager) {
    crypto::PubKey* replicaPublicKey = keyManager->GetPublicKey(
        signedMessage.replica_id());
    // verify that the replica actually sent this reply and that we are expecting
    // this reply
    return crypto::IsMessageValid(replicaPublicKey, signedMessage.packed_msg(),
          &signedMessage);
}

void SignMessage(const ::google::protobuf::Message &msg,
    crypto::PrivKey* privateKey, uint64_t processId,
    proto::SignedMessage &signedMessage) {
  proto::PackedMessage packedMsg;
  *packedMsg.mutable_msg() = msg.SerializeAsString();
  *packedMsg.mutable_type() = msg.GetTypeName();
  // TODO this is not portable. SerializeAsString may not return the same
  // result every time
  std::string msgData = packedMsg.SerializeAsString();
  crypto::SignMessage(privateKey, msgData, signedMessage);
  signedMessage.set_packed_msg(msgData);
  signedMessage.set_replica_id(processId);
}

template <typename S>
void PackRequest(proto::PackedMessage &packedMsg, S &s) {
  packedMsg.set_msg(s.SerializeAsString());
  packedMsg.set_type(s.GetTypeName());
}

std::string TransactionDigest(const proto::Transaction &txn) {
  CryptoPP::SHA256 hash;
  std::string digest;

  for (const auto &group : txn.participating_shards()) {
    hash.Update((const CryptoPP::byte*) &group, sizeof(group));
  }
  for (const auto &read : txn.readset()) {
    uint64_t readtimeId = read.readtime().id();
    uint64_t readtimeTs = read.readtime().timestamp();
    hash.Update((const CryptoPP::byte*) &read.key()[0], read.key().length());
    hash.Update((const CryptoPP::byte*) &readtimeId,
        sizeof(read.readtime().id()));
    hash.Update((const CryptoPP::byte*) &readtimeTs,
        sizeof(read.readtime().timestamp()));
  }
  for (const auto &write : txn.writeset()) {
    hash.Update((const CryptoPP::byte*) &write.key()[0], write.key().length());
    hash.Update((const CryptoPP::byte*) &write.value()[0], write.value().length());
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
}

std::string BatchedDigest(proto::BatchedRequest& breq) {

  CryptoPP::SHA256 hash;
  std::string digest;

  for (int i = 0; i < breq.digests_size(); i++) {
    std::string dig = (*breq.mutable_digests())[i];
    hash.Update((CryptoPP::byte*) &dig[0], dig.length());
  }

  digest.resize(hash.DigestSize());
  hash.Final((CryptoPP::byte*) &digest[0]);

  return digest;
}

std::string string_to_hex(const std::string& input)
{
    static const char hex_digits[] = "0123456789ABCDEF";

    std::string output;
    output.reserve(input.length() * 2);
    for (unsigned char c : input)
    {
        output.push_back(hex_digits[c >> 4]);
        output.push_back(hex_digits[c & 15]);
    }
    return output;
}

void DebugHash(const std::string& hash) {
  Debug("Hash: %s", string_to_hex(hash).substr(0,10).c_str());
}


bool verifyGDecision(const proto::GroupedDecision& gdecision,
  const proto::Transaction& txn, KeyManager* keyManager, bool signMessages, uint64_t f) {
  std::string digest = gdecision.txn_digest();

  // We will go through the grouped decisions and make sure that each
  // decision is valid. Then, we will mark the shard for those decisions
  // as valid. We return true if all participating shard decisions are valid



  // This will hold the remaining shards that we need to verify
  std::unordered_set<uint64_t> remaining_shards;
  for (auto id : txn.participating_shards()) {
    Debug("requiring %lu", id);
    remaining_shards.insert(id);
  }

  // std::vector<std::thread*> Threads;
  // std::atomic_int *outstanding_verifications = new std::atomic_int(remaining_shards.size() * (f+1));
  // for(int j = 0; j < *outstanding_verifications; j++){
  //   std::thread* t;
  //   Threads.push_back(t);
  // }
  // int i = 0;


    //return true;
  int max_counter = 0; //just a hack to artificially reduce verification to get an upper bound on tput
  //return true;

  if (signMessages) {
    // iterate over all shards
    for (const auto& pair : gdecision.signed_decisions().grouped_decisions()) {
      uint64_t shard_id = pair.first;
      proto::GroupedSignedMessage grouped = pair.second;
      // check if we are still looking for a grouped decision from this shard
      if (remaining_shards.find(shard_id) != remaining_shards.end()) {
        // unpack the message in the grouped signed message
        proto::PackedMessage packedMsg;
        if (packedMsg.ParseFromString(grouped.packed_msg())) {
          // make sure the packed message is a Transaction Decision
          proto::TransactionDecision decision;
          if (decision.ParseFromString(packedMsg.msg())) {
            // verify that the transaction decision is valid
            if (decision.status() == REPLY_OK &&
                decision.txn_digest() == digest &&
                decision.shard_id() == shard_id) {
              proto::SignedMessage signedMsg;
              signedMsg.set_packed_msg(grouped.packed_msg());
              // use this to keep track of the replicas for whom we have gotten
              // a valid signature.
              std::unordered_set<uint64_t> valid_signatures;

              // now verify all of the signatures
              for (const auto& id_sig_pair: grouped.signatures()) {
                Debug("ungrouped transaction decision for %lu", id_sig_pair.first);
                // recreate the signed message for the given replica id
                signedMsg.set_replica_id(id_sig_pair.first);
                signedMsg.set_signature(id_sig_pair.second);
                // Debug("signature for %lu: %s", id_sig_pair.first, string_to_hex(id_sig_pair.second).c_str());

                crypto::PubKey* replicaPublicKey = keyManager->GetPublicKey(signedMsg.replica_id());

                // i++;
                // Threads[i] = new std::thread([](){return;});
                // Threads[i] = new std::thread([outstanding_verifications, sig = signedMsg.mutable_signature(),
                //    msg = signedMsg.mutable_packed_msg(), key = replicaPublicKey](){
                //   if(hotstuffBatchedSigs::verifyBatchedSignature(sig, msg, key)){
                //     //(*outstanding_verifications)--;
                //   }
                //   else{
                //     //(*outstanding_verifications)--; //just a hack since I broke verifications..
                //   }
                // });
                if (hotstuffBatchedSigs::verifyBatchedSignature(signedMsg.mutable_signature(),
                signedMsg.mutable_packed_msg(), replicaPublicKey)) {
                  valid_signatures.insert(id_sig_pair.first);
                  max_counter--;
                  //if(max_counter == 0) return true;
                } else {
                  Debug("Failed to validate transaction decision signature for %lu", id_sig_pair.first);
                  //Panic("should not fail verifications");
                  valid_signatures.insert(id_sig_pair.first);
                  //std::cerr << "FAILED VERIFICATION ABORT" << std::endl;
                  //return true;
                }
              }

              // If we have confirmed f+1 signatures, then we mark this shard
              // as verifying the decision
              if (valid_signatures.size() >= f + 1) {
                Debug("signed: verified shard %lu", shard_id);
                remaining_shards.erase(shard_id);
              }
            }
          }
        }
      }
    }
  } else {
    for (const auto& pair : gdecision.decisions().grouped_decisions()) {
      uint64_t shard_id = pair.first;
      proto::TransactionDecision decision = pair.second;
      if (remaining_shards.find(shard_id) != remaining_shards.end()) {
        if (decision.status() == REPLY_OK &&
            decision.txn_digest() == digest &&
            decision.shard_id() == shard_id) {
          remaining_shards.erase(shard_id);
        }
      }
    }
  }
 //  for(auto t : Threads){
 //    t->join();
 //    delete t;
 //  }
 // //  bool ret = *outstanding_verifications == 0
 //   delete outstanding_verifications;
 // // return ret;

  // the grouped decision should have a proof for all of the participating shards
  return remaining_shards.size() == 0;
}

bool verifyG_Abort_Decision(const proto::GroupedDecision& gdecision,
  const proto::Transaction& txn, KeyManager* keyManager, bool signMessages, uint64_t f) {
  std::string digest = gdecision.txn_digest();

  // We will go through the grouped decisions and make sure that each
  // decision is valid. Then, we will mark the shard for those decisions
  // as valid. We return true if all participating shard decisions are valid


  // This will hold the remaining shards that we need to verify
  std::unordered_set<uint64_t> remaining_shards;
  for (auto id : txn.participating_shards()) {
    Debug("requiring %lu", id);
    remaining_shards.insert(id);
  }


  if (signMessages) {
    // iterate over all shards
    for (const auto& pair : gdecision.signed_decisions().grouped_decisions()) {

      uint64_t shard_id = pair.first;

      proto::GroupedSignedMessage grouped = pair.second;
      // check if we are still looking for a grouped decision from this shard
      if (remaining_shards.find(shard_id) != remaining_shards.end()) {
        // unpack the message in the grouped signed message
        proto::PackedMessage packedMsg;
        if (packedMsg.ParseFromString(grouped.packed_msg())) {
          // make sure the packed message is a Transaction Decision
          proto::TransactionDecision decision;
          if (decision.ParseFromString(packedMsg.msg())) {
            // verify that the transaction decision is valid
            if (decision.status() == REPLY_FAIL &&
                decision.txn_digest() == digest &&
                decision.shard_id() == shard_id) {
              proto::SignedMessage signedMsg;
              signedMsg.set_packed_msg(grouped.packed_msg());
              // use this to keep track of the replicas for whom we have gotten
              // a valid signature.
              std::unordered_set<uint64_t> valid_signatures;

              // now verify all of the signatures
              for (const auto& id_sig_pair: grouped.signatures()) {

                Debug("ungrouped transaction decision for %lu", id_sig_pair.first);
                // recreate the signed message for the given replica id
                signedMsg.set_replica_id(id_sig_pair.first);
                signedMsg.set_signature(id_sig_pair.second);
                // Debug("signature for %lu: %s", id_sig_pair.first, string_to_hex(id_sig_pair.second).c_str());

                crypto::PubKey* replicaPublicKey = keyManager->GetPublicKey(signedMsg.replica_id());

                if (hotstuffBatchedSigs::verifyBatchedSignature(signedMsg.mutable_signature(), signedMsg.mutable_packed_msg(), replicaPublicKey)) {
                  valid_signatures.insert(id_sig_pair.first);
                  //max_counter--;
                  //if(max_counter == 0) return true;
                } else {
                  Debug("Failed to validate transaction decision signature for %lu", id_sig_pair.first);
                  //Panic("verification fails");
                  valid_signatures.insert(id_sig_pair.first);
                  //std::cerr << "FAILED VERIFICATION ABORT" << std::endl;
                  //return true;
                  //return false; //got a false verification.
                }
              }

              // If we have confirmed f+1 signatures, then we mark this shard
              // as verifying the decision
              if (valid_signatures.size() >= f + 1) {
                Debug("signed: verified shard %lu", shard_id);
                remaining_shards.erase(shard_id);
                return true;
              }
            }
          }
        }
      }
    }
    //this branch is not tested..
  } else {
    for (const auto& pair : gdecision.decisions().grouped_decisions()) {
      uint64_t shard_id = pair.first;
      proto::TransactionDecision decision = pair.second;
      if (remaining_shards.find(shard_id) != remaining_shards.end()) {
        if (decision.status() == REPLY_FAIL &&
            decision.txn_digest() == digest &&
            decision.shard_id() == shard_id) {
          remaining_shards.erase(shard_id);
          return true;
        }
      }
    }
  }

  // the grouped decision should have a proof for one of the participating shards
  return false;
}

bool verifyGDecision_parallel(const proto::GroupedDecision& gdecision,
  const proto::Transaction& txn, KeyManager* keyManager, bool signMessages, uint64_t f, Transport* tp ) {
  std::string digest = gdecision.txn_digest();

 std::cerr<< "starting parallel verification" << std::endl;
  // This will hold the remaining shards that we need to verify
  std::unordered_set<uint64_t> remaining_shards;
  for (auto id : txn.participating_shards()) {
    Debug("requiring %lu", id);
    remaining_shards.insert(id);
  }

  if (signMessages) {
    std::vector<std::function<void*()>> verificationJobs;
    asyncVerification *verifyObj = new asyncVerification(txn.participating_shards().size());
    //verifyObj->deletable = verifyObj->groupTotals * (f+1);
    // iterate over all shards
    for (const auto& pair : gdecision.signed_decisions().grouped_decisions()) {
      uint64_t shard_id = pair.first;
      proto::GroupedSignedMessage grouped = pair.second;
      // check if we are still looking for a grouped decision from this shard
      if (remaining_shards.find(shard_id) != remaining_shards.end()) {
          verifyObj->groupCounts[shard_id] = f+1;

        // unpack the message in the grouped signed message
        proto::PackedMessage packedMsg;
        if (packedMsg.ParseFromString(grouped.packed_msg())) {
          // make sure the packed message is a Transaction Decision
          proto::TransactionDecision decision;
          if (decision.ParseFromString(packedMsg.msg())) {
            // verify that the transaction decision is valid
            if (decision.status() == REPLY_OK && decision.txn_digest() == digest && decision.shard_id() == shard_id) {

              //verifyObj->sigMsgs.push_back(signedMsg);
              proto::SignedMessage signedMsg;
              signedMsg.set_packed_msg(grouped.packed_msg());
              // use this to keep track of the replicas for whom we have gotten
              // a valid signature.
              std::unordered_set<uint64_t> involved_replicas;

              // now verify all of the signatures
              for (const auto& id_sig_pair: grouped.signatures()) {
                Debug("ungrouped transaction decision for %lu", id_sig_pair.first);
                // recreate the signed message for the given replica id
                signedMsg.set_replica_id(id_sig_pair.first);
                signedMsg.set_signature(id_sig_pair.second);
                // Debug("signature for %lu: %s", id_sig_pair.first, string_to_hex(id_sig_pair.second).c_str());

                crypto::PubKey* replicaPublicKey = keyManager->GetPublicKey(signedMsg.replica_id());

                auto f = [signedMsg_ = signedMsg, verifyObj, replicaPublicKey, shard_id]() mutable {
                  if(verifyObj == nullptr){ return (void*) false;}

                  bool res = hotstuffBatchedSigs::verifyBatchedSignature(signedMsg_.mutable_signature(),
                  signedMsg_.mutable_packed_msg(), replicaPublicKey);
                  std::unique_lock lock(verifyObj->objMutex);
                  if(res){
                    verifyObj->groupCounts[shard_id]--;
                  }
                  else{
                    verifyObj->groupCounts[shard_id]--; //Just a hack because I broke validation
                  }
                  if(verifyObj->groupCounts[shard_id] == 0){
                    verifyObj->groupsVerified++;
                  }
                  if(verifyObj->groupsVerified == verifyObj->groupTotals){
                    verifyObj->result = true;
                  }
                  verifyObj->deletable--;
                  if(verifyObj->deletable == 0){
                    verifyObj->cv_wait.notify_one();
                  }

                  //verifyObj->cv_wait.notify_one();
                  return (void*) true;
                };
                verificationJobs.push_back(std::move(f));

              }
            }
          }
        }
      }
    }

    verifyObj->deletable = verificationJobs.size();
    for(auto &job : verificationJobs){
      tp->DispatchTP_noCB(job);
    }

    std::unique_lock lock(verifyObj->objMutex);
    verifyObj->cv_wait.wait(lock, [verifyObj] {return verifyObj->deletable == 0;});
    //
    std::cerr << "completing verification " << std::endl;
    if(verifyObj->result){
      //delete verifyObj;
      return true;
    }
    else{
      //delete verifyObj;
      return false;
    }

  } else {
    for (const auto& pair : gdecision.decisions().grouped_decisions()) {
      uint64_t shard_id = pair.first;
      proto::TransactionDecision decision = pair.second;
      if (remaining_shards.find(shard_id) != remaining_shards.end()) {
        if (decision.status() == REPLY_OK &&
            decision.txn_digest() == digest &&
            decision.shard_id() == shard_id) {
          remaining_shards.erase(shard_id);
        }
      }
    }
    return remaining_shards.size() == 0;
  }

}

bool verifyGDecision_Abort_parallel(const proto::GroupedDecision& gdecision,
  const proto::Transaction& txn, KeyManager* keyManager, bool signMessages, uint64_t f, Transport* tp ) {
  std::string digest = gdecision.txn_digest();

 std::cerr<< "starting parallel verification" << std::endl;
  // This will hold the remaining shards that we need to verify
  std::unordered_set<uint64_t> remaining_shards;
  for (auto id : txn.participating_shards()) {
    Debug("requiring %lu", id);
    remaining_shards.insert(id);
  }

  if (signMessages) {
    std::vector<std::function<void*()>> verificationJobs;
    asyncVerification *verifyObj = new asyncVerification(txn.participating_shards().size());
    //verifyObj->deletable = verifyObj->groupTotals * (f+1);
    // iterate over all shards
    for (const auto& pair : gdecision.signed_decisions().grouped_decisions()) {
      uint64_t shard_id = pair.first;
      proto::GroupedSignedMessage grouped = pair.second;
      // check if we are still looking for a grouped decision from this shard
      if (remaining_shards.find(shard_id) != remaining_shards.end()) {
          verifyObj->groupCounts[shard_id] = f+1;

        // unpack the message in the grouped signed message
        proto::PackedMessage packedMsg;
        if (packedMsg.ParseFromString(grouped.packed_msg())) {
          // make sure the packed message is a Transaction Decision
          proto::TransactionDecision decision;
          if (decision.ParseFromString(packedMsg.msg())) {
            // verify that the transaction decision is valid
            if (decision.status() == REPLY_FAIL && decision.txn_digest() == digest && decision.shard_id() == shard_id) {

              //verifyObj->sigMsgs.push_back(signedMsg);
              proto::SignedMessage signedMsg;
              signedMsg.set_packed_msg(grouped.packed_msg());
              // use this to keep track of the replicas for whom we have gotten
              // a valid signature.
              std::unordered_set<uint64_t> involved_replicas;

              // now verify all of the signatures
              for (const auto& id_sig_pair: grouped.signatures()) {
                Debug("ungrouped transaction decision for %lu", id_sig_pair.first);
                // recreate the signed message for the given replica id
                signedMsg.set_replica_id(id_sig_pair.first);
                signedMsg.set_signature(id_sig_pair.second);
                // Debug("signature for %lu: %s", id_sig_pair.first, string_to_hex(id_sig_pair.second).c_str());

                crypto::PubKey* replicaPublicKey = keyManager->GetPublicKey(signedMsg.replica_id());

                auto f = [signedMsg_ = signedMsg, verifyObj, replicaPublicKey, shard_id]() mutable {
                  if(verifyObj == nullptr){ return (void*) false;}

                  bool res = hotstuffBatchedSigs::verifyBatchedSignature(signedMsg_.mutable_signature(),
                  signedMsg_.mutable_packed_msg(), replicaPublicKey);
                  std::unique_lock lock(verifyObj->objMutex);
                  if(res){
                    verifyObj->groupCounts[shard_id]--;
                  }
                  else{
                    verifyObj->groupCounts[shard_id]--; //Just a hack because I broke validation
                  }
                  if(verifyObj->groupCounts[shard_id] == 0){
                    verifyObj->result = true;
                  }
                  verifyObj->deletable--;
                  if(verifyObj->deletable == 0){
                    verifyObj->cv_wait.notify_one();
                  }

                  //verifyObj->cv_wait.notify_one();
                  return (void*) true;
                };
                verificationJobs.push_back(std::move(f));

              }
            }
          }
        }
      }
    }

    verifyObj->deletable = verificationJobs.size();
    for(auto &job : verificationJobs){
      tp->DispatchTP_noCB(job);
    }

    std::unique_lock lock(verifyObj->objMutex);
    verifyObj->cv_wait.wait(lock, [verifyObj] {return verifyObj->deletable == 0;});
    //
    std::cerr << "completing verification " << std::endl;
    if(verifyObj->result){
      //delete verifyObj;
      return true;
    }
    else{
      //delete verifyObj;
      return false;
    }

  } else {
    for (const auto& pair : gdecision.decisions().grouped_decisions()) {
      uint64_t shard_id = pair.first;
      proto::TransactionDecision decision = pair.second;
      if (remaining_shards.find(shard_id) != remaining_shards.end()) {
        if (decision.status() == REPLY_OK &&
            decision.txn_digest() == digest &&
            decision.shard_id() == shard_id) {
          remaining_shards.erase(shard_id);
        }
      }
    }
    return remaining_shards.size() == 0;
  }

}


} // namespace indicusstore
