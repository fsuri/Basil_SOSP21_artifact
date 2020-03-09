#ifndef INDICUS_COMMON_H
#define INDICUS_COMMON_H

#include "bft_tapir/config.h"
#include "lib/configuration.h"
#include "store/indicusstore/indicus-proto.pb.h"

#include <map>
#include <string>
#include <vector>

#include <google/protobuf/message.h>

namespace indicusstore {

bool ValidateSignedMessage(const proto::SignedMessage &signedMessage,
    const bft_tapir::NodeConfig *cryptoConfig);

void SignMessage(const ::google::protobuf::Message &msg,
    const crypto::PrivKey &privateKey, uint64_t processId,
    proto::SignedMessage &signedMessage);

proto::CommitDecision IndicusDecide(const std::vector<proto::Phase1Reply> &replies,
    const transport::Configuration *config);

} // namespace indicusstore

#endif /* INDICUS_COMMON_H */
