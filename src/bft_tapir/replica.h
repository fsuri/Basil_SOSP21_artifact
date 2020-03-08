
#ifndef _NODE_H_
#define _NODE_H_

#include <memory>

#include "lib/assert.h"
#include "lib/configuration.h"
#include "lib/crypto.h"
#include "lib/message.h"
#include "lib/persistent_register.h"
#include "lib/udptransport.h"

#include "bft_tapir/config.h"
#include "bft_tapir/messages-proto.pb.h"

namespace bft_tapir {

class Replica : TransportReceiver {
 public:
  Replica(NodeConfig config, int myId, Transport *transport);
  ~Replica();

  // Message handlers.
  void ReceiveMessage(const TransportAddress &remote, const std::string &type,
                      const std::string &data, void* metadata);

 private:
  NodeConfig config;
  int myId;  // Replica index into config.
  Transport *transport;
  int view;
  crypto::PrivKey privateKey;
  int seqnum;

  int getPrimaryForView(int view);
};

}  // namespace bft_tapir

#endif /* _NODE_H_ */