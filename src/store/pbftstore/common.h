#ifndef PBFT_COMMON_H
#define PBFT_COMMON_H

#include "lib/configuration.h"
#include "lib/keymanager.h"
#include "store/common/timestamp.h"
#include "store/pbftstore/pbft-proto.pb.h"
#include "store/pbftstore/server-proto.pb.h"

#include <map>
#include <string>
#include <vector>

#include <google/protobuf/message.h>

namespace pbftstore {

bool ValidateSignedMessage(const proto::SignedMessage &signedMessage,
    KeyManager *keyManager, ::google::protobuf::Message &plaintextMsg);

bool ValidateSignedMessage(const proto::SignedMessage &signedMessage,
    KeyManager *keyManager, std::string &data, std::string &type);

bool __PreValidateSignedMessage(const proto::SignedMessage &signedMessage,
    KeyManager *keyManager, proto::PackedMessage &packedMessage);

bool CheckSignature(const proto::SignedMessage &signedMessage,
    KeyManager *keyManager);

void SignMessage(const ::google::protobuf::Message &msg,
    crypto::PrivKey* privateKey, uint64_t processId,
    proto::SignedMessage &signedMessage);

std::string TransactionDigest(const proto::Transaction &txn);

std::string BatchedDigest(proto::BatchedRequest& breq);

std::string string_to_hex(const std::string& input);

// return true if the grouped decision is valid
bool verifyGDecision(const proto::GroupedDecision& gdecision,
  const proto::Transaction& txn, KeyManager* keyManager, bool signMessages, uint64_t f);

} // namespace pbftstore

#endif /* PBFT_COMMON_H */
