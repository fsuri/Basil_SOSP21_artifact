#include "store/mortystore/replica.h"

namespace mortystore {

Replica::Replica(const transport::Configuration &config, int idx,
    Transport *transport) : replication::Replica(config, idx, transport,
      nullptr) {
}

Replica::~Replica() {
}

void Replica::ReceiveMessage(const TransportAddress &remote,
      const std::string &type, const std::string &data) {
}

void Replica::Load(const std::string &key, const std::string &value,
    const Timestamp timestamp) {
}

} // namespace mortystore
