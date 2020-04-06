#ifndef _PBFT_REPLICA_H_
#define _PBFT_REPLICA_H_

#include <memory>

#include "lib/assert.h"
#include "lib/configuration.h"
#include "lib/message.h"
#include "lib/persistent_register.h"
#include "lib/udptransport.h"
#include "lib/crypto.h"
#include "lib/keymanager.h"

#include "store/pbftstore/pbft-proto.pb.h"
#include "store/pbftstore/slots.h"
#include "store/pbftstore/app.h"
#include "store/pbftstore/common.h"

namespace pbftstore {

class Replica : public TransportReceiver {
public:
  Replica(const transport::Configuration &config, KeyManager *keyManager,
    App *app, int groupIdx, int myId, bool signMessages, Transport *transport);
  ~Replica();

  // Message handlers.
  void ReceiveMessage(const TransportAddress &remote, const std::string &type,
                      const std::string &data, void *meta_data);
  void HandleRequest(const TransportAddress &remote,
                           const proto::Request &msg);
  void HandlePreprepare(const TransportAddress &remote,
                              const proto::Preprepare &msg, uint64_t replica_id);
  void HandlePrepare(const TransportAddress &remote,
                           const proto::Prepare &msg, uint64_t replica_id);
  void HandleCommit(const TransportAddress &remote,
                          const proto::Commit &msg, uint64_t replica_id);

 private:
  const transport::Configuration &config;
  KeyManager *keyManager;
  App *app;
  int groupIdx;
  int myId;  // Replica index into config.
  bool signMessages;
  Transport *transport;
  int view;
  int seqnum;

  int getPrimaryForView(int view);

  Slots slots;
};

} // namespace pbftstore

#endif
