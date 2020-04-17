#include "store/pbftstore/common.h"

#include <cryptopp/sha.h>

#include "store/common/timestamp.h"
#include "store/common/transaction.h"

namespace pbftstore {

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
      signedMessage.replica_id());
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
  signedMessage.set_replica_id(processId);
}

template <typename S>
void PackRequest(proto::PackedMessage &packedMsg, S &s) {
  packedMsg.set_msg(s.SerializeAsString());
  packedMsg.set_type(s.GetTypeName());
}

// std::string RequestDigest(const proto::Request &request) {
//   CryptoPP::SHA256 hash;
//   std::string digest;
//
//   hash.Update((const byte*) &request.packed_msg().type()[0], request.packed_msg().type().length());
//   hash.Update((const byte*) &request.packed_msg().msg()[0], request.packed_msg().msg().length());
//
//   digest.resize(hash.DigestSize());
//   hash.Final((byte*) &digest[0]);
//
//   return digest;
// }

std::string TransactionDigest(const proto::Transaction &txn) {
  CryptoPP::SHA256 hash;
  std::string digest;

  for (const auto &group : txn.participating_shards()) {
    hash.Update((const byte*) &group, sizeof(group));
  }
  for (const auto &read : txn.readset()) {
    uint64_t readtimeId = read.readtime().id();
    uint64_t readtimeTs = read.readtime().timestamp();
    hash.Update((const byte*) &read.key()[0], read.key().length());
    hash.Update((const byte*) &readtimeId,
        sizeof(read.readtime().id()));
    hash.Update((const byte*) &readtimeTs,
        sizeof(read.readtime().timestamp()));
  }
  for (const auto &write : txn.writeset()) {
    hash.Update((const byte*) &write.key()[0], write.key().length());
    hash.Update((const byte*) &write.value()[0], write.value().length());
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

std::string BatchedDigest(proto::BatchedRequest& breq) {

  CryptoPP::SHA256 hash;
  std::string digest;

  for (int i = 0; i < breq.digests_size(); i++) {
    std::string dig = (*breq.mutable_digests())[i];
    hash.Update((byte*) &dig[0], dig.length());
  }

  digest.resize(hash.DigestSize());
  hash.Final((byte*) &digest[0]);

  return digest;
}

} // namespace indicusstore
