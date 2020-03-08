
#ifndef _NODE_CONFIG_H_
#define _NODE_CONFIG_H_

#include <map>
#include <string>

#include "lib/configuration.h"
#include "lib/crypto.h"

namespace bft_tapir {

using namespace std;

class NodeConfig {
 public:
  NodeConfig(transport::Configuration replicaConfigStream,
             transport::Configuration clientConfigStream, string keyPath, int n,
             int f, int c);

  ~NodeConfig();

  const crypto::PubKey &getReplicaPublicKey(int id) const;
  const crypto::PubKey &getClientPublicKey(int id) const;
  transport::Configuration getReplicaConfig();
  transport::ReplicaAddress getClientAddress(int id);
  crypto::PrivKey getClientPrivateKey(int id);
  crypto::PrivKey getReplicaPrivateKey(int id);

  bool isValidClientId(int id);
  bool isValidReplicaId(int id);
  int getF();
  int getN();

 private:
  transport::Configuration replicaConfig;
  transport::Configuration clientConfig;
  string keyPath;
  int n;
  int f;
  int c;  // number of clients
  map<int, crypto::PubKey> replicaPublicKeys;
  map<int, crypto::PubKey> clientPublicKeys;
};

}  // namespace bft_tapir

#endif /* _NODE_CONFIG_H_ */
