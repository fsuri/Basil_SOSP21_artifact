#ifndef INDICUS_COMMON_H
#define INDICUS_COMMON_H

#include "lib/configuration.h"
#include "lib/keymanager.h"
#include "store/common/timestamp.h"
#include "store/indicusstore/indicus-proto.pb.h"

#include <map>
#include <string>
#include <vector>

#include <google/protobuf/message.h>

namespace indicusstore {

bool ValidateSignedMessage(const proto::SignedMessage &signedMessage,
    KeyManager *keyManager, ::google::protobuf::Message &plaintextMsg);

bool ValidateSignedMessage(const proto::SignedMessage &signedMessage,
    KeyManager *keyManager, std::string &data, std::string &type);

bool __PreValidateSignedMessage(const proto::SignedMessage &signedMessage,
    KeyManager *keyManager, proto::PackedMessage &packedMessage);

void SignMessage(const ::google::protobuf::Message &msg,
    const crypto::PrivKey &privateKey, uint64_t processId,
    proto::SignedMessage &signedMessage);

proto::CommitDecision IndicusDecide(
    const std::map<int, std::vector<proto::Phase1Reply>> &replies,
    const transport::Configuration *config, bool validateProofs,
    bool signedMessages, KeyManager *keyManager);

proto::CommitDecision IndicusShardDecide(
    const std::vector<proto::Phase1Reply> &replies,
    const transport::Configuration *config, bool validateProofs,
    bool signedMessages, KeyManager *keyManager, bool &fast);

bool ValidateTransactionWrite(const proto::CommittedProof &proof,
    const std::string &key, const std::string &val, const Timestamp &timestamp,
    const transport::Configuration *config, bool signedMessages,
    KeyManager *keyManager);

// check must validate that proof replies are from all involved shards
bool ValidateProof(const proto::CommittedProof &proof,
    const transport::Configuration *config, bool signedMessages,
    KeyManager *keyManager);

bool ValidateP1RepliesCommit(
    const std::map<int, std::vector<proto::Phase1Reply>> &groupedP1Replies,
    const std::string &txnDigest, const proto::Transaction &txn,
    const transport::Configuration *config);

bool ValidateP2RepliesCommit(
    const std::vector<proto::Phase2Reply> &p2Replies,
    const std::string &txnDigest, const proto::Transaction &txn,
    const transport::Configuration *config);

bool ValidateDependency(const proto::Dependency &dep,
    const transport::Configuration *config, bool signedMessages,
    KeyManager *keyManager);

bool operator==(const proto::PreparedWrite &pw1, const proto::PreparedWrite &pw2);

bool operator!=(const proto::PreparedWrite &pw1, const proto::PreparedWrite &pw2);

std::string TransactionDigest(const proto::Transaction &txn);

std::string BytesToHex(const std::string &bytes, size_t maxLength);

} // namespace indicusstore

#endif /* INDICUS_COMMON_H */
