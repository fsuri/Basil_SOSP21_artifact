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
    KeyManager *keyManager);

void SignMessage(const ::google::protobuf::Message &msg,
    const crypto::PrivKey &privateKey, uint64_t processId,
    proto::SignedMessage &signedMessage);

proto::CommitDecision IndicusDecide(
    const std::map<int, std::vector<proto::Phase1Reply>> &replies,
    const transport::Configuration *config, bool validateProofs);

proto::CommitDecision IndicusShardDecide(
    const std::vector<proto::Phase1Reply> &replies,
    const transport::Configuration *config, bool validateProofs, bool &fast);

bool ValidateCommittedProof(const proto::CommittedProof &proof,
    const std::string &key, const std::string &val, const Timestamp &timestamp);

// check must validate that proof replies are from all involved shards
bool ValidateProof(const proto::CommittedProof &proof);

uint64_t TransactionDigest(const proto::Transaction &txn);

} // namespace indicusstore

#endif /* INDICUS_COMMON_H */
