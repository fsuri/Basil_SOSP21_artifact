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
    App *app, int groupIdx, int idx, bool signMessages, uint64_t maxBatchSize,
    uint64_t batchTimeoutMS, bool primaryCoordinator, Transport *transport);
  ~Replica();

  // Message handlers.
  void ReceiveMessage(const TransportAddress &remote, const std::string &type,
                      const std::string &data, void *meta_data);
  void HandleRequest(const TransportAddress &remote,
                           const proto::Request &msg);
  void HandleBatchedRequest(const TransportAddress &remote,
                           proto::BatchedRequest &msg);
  void HandlePreprepare(const TransportAddress &remote,
                              const proto::Preprepare &msg,
                            const proto::SignedMessage& signedMsg);
  void HandlePrepare(const TransportAddress &remote,
                           const proto::Prepare &msg,
                         const proto::SignedMessage& signedMsg);
  void HandleCommit(const TransportAddress &remote,
                          const proto::Commit &msg,
                        const proto::SignedMessage& signedMsg);
  void HandleGrouped(const TransportAddress &remote,
                          const proto::GroupedSignedMessage &msg);

 private:
  const transport::Configuration &config;
  KeyManager *keyManager;
  App *app;
  int groupIdx;
  int idx; // the replica index within the group
  int id; // unique replica id (across all shards)
  bool signMessages;
  uint64_t maxBatchSize;
  uint64_t batchTimeoutMS;
  bool primaryCoordinator;
  Transport *transport;
  int currentView;
  int nextSeqNum;

  Slots slots;

  bool batchTimerRunning;
  int batchTimerId;
  int nextBatchNum;
  // the map from 0..(N-1) to pending digests
  std::unordered_map<uint64_t, std::string> pendingBatchedDigests;
  void sendBatchedPreprepare();

  bool sendMessageToAll(const ::google::protobuf::Message& msg);
  bool sendMessageToPrimary(const ::google::protobuf::Message& msg);

  // map from batched digest to received batched requests
  std::unordered_map<std::string, proto::BatchedRequest> batchedRequests;
  // map from digest to received requests
  std::unordered_map<std::string, proto::PackedMessage> requests;

  // the next sequence number to be executed
  uint64_t execSeqNum;
  uint64_t execBatchNum;
  // map from seqnum to the digest pending execution at that sequence number
  std::unordered_map<uint64_t, std::string> pendingExecutions;

  void SendPreprepare(uint64_t seqnum, const proto::Preprepare& preprepare);
  // map from seqnum to timer ids. If the primary commits the sequence number
  // before the timer expires, then it cancels the timer
  std::unordered_map<uint64_t, int> seqnumCommitTimers;

  // map from tx digest to reply address
  std::unordered_map<std::string, TransportAddress*> replyAddrs;

  // tests to see if we are ready to send commit or executute the slot
  void testSlot(uint64_t seqnum, uint64_t viewnum, std::string digest, bool gotPrepare);

  void executeSlots();
};

} // namespace pbftstore

#endif
