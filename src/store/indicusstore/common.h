#ifndef INDICUS_COMMON_H
#define INDICUS_COMMON_H

#include "bft_tapir/config.h"
#include "lib/configuration.h"
#include "store/indicusstore/indicus-proto.pb.h"

#include <map>
#include <string>
#include <vector>

namespace indicusstore {

bool ValidateSignedMessage(const proto::SignedMessage &signedMessage,
    const bft_tapir::NodeConfig *cryptoConfig);

proto::TxnAction IndicusDecide(const std::vector<proto::Prepare1Reply> &replies,
    const transport::Configuration *config);

} // namespace indicusstore

#endif /* INDICUS_COMMON_H */
