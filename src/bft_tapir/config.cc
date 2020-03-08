#include "bft_tapir/config.h"

#include "lib/assert.h"

namespace bft_tapir {

using namespace std;

NodeConfig::NodeConfig(transport::Configuration replicaConfig,
                       transport::Configuration clientConfig,
                       const string keyPath, int n, int f, int c)
    : replicaConfig(std::move(replicaConfig)),
      clientConfig(std::move(clientConfig)),
      keyPath(keyPath),
      n(n),
      f(f),
      c(c) {
  for (int i = 0; i < n; i++) {
    replicaPublicKeys[i] =
        crypto::LoadPublicKey(keyPath + "/replica" + to_string(i) + ".pub");
  }
  for (int i = 0; i < c; i++) {
    clientPublicKeys[i] =
        crypto::LoadPublicKey(keyPath + "/client" + to_string(i) + ".pub");
  }
}

NodeConfig::~NodeConfig() {}

const crypto::PubKey &NodeConfig::getReplicaPublicKey(int id) const {
  auto itr = replicaPublicKeys.find(id);
  UW_ASSERT(itr != replicaPublicKeys.end());
  return itr->second;
}
const crypto::PubKey &NodeConfig::getClientPublicKey(int id) const {
  auto itr = clientPublicKeys.find(id);
  UW_ASSERT(itr != clientPublicKeys.end());
  return itr->second;
}
transport::Configuration NodeConfig::getReplicaConfig() {
  return replicaConfig;
}
transport::ReplicaAddress NodeConfig::getClientAddress(int id) {
  return clientConfig.replica(0, id);
}
crypto::PrivKey NodeConfig::getClientPrivateKey(int id) {
  return crypto::LoadPrivateKey(keyPath + "/client" + to_string(id) + ".priv");
}
crypto::PrivKey NodeConfig::getReplicaPrivateKey(int id) {
  return crypto::LoadPrivateKey(keyPath + "/replica" + to_string(id) + ".priv");
}

bool NodeConfig::isValidClientId(int id) { return id >= 0 && id < c; }

bool NodeConfig::isValidReplicaId(int id) { return id >= 0 && id < n; }

int NodeConfig::getN() { return n; }

int NodeConfig::getF() { return f; }

}  // namespace bft_tapir
