#ifndef _BFTSMART_REPLICA_H_
#define _BFTSMART_REPLICA_H_

#include <memory>

#include "lib/assert.h"
#include "lib/configuration.h"
#include "lib/message.h"
#include "lib/persistent_register.h"
#include "lib/udptransport.h"
#include "lib/tcptransport.h"
#include "lib/crypto.h"
#include "lib/keymanager.h"

#include "store/common/stats.h"
#include "store/bftsmartstore/pbft-proto.pb.h"
#include "store/bftsmartstore/server-proto.pb.h"
#include "store/bftsmartstore/slots.h"
#include "store/bftsmartstore/app.h"
#include "store/bftsmartstore/common.h"
#include <mutex>
#include "tbb/concurrent_unordered_map.h"

#include "store/bftsmartstore/bftsmartagent.h"

// use HotStuff library
// comment out the below macro to switch back to pbftstore
#include "store/bftsmartstore/libhotstuff/examples/indicus_interface.h"

namespace bftsmartstore {

class Replica : public TransportReceiver {
public:
  Replica(const transport::Configuration &config, KeyManager *keyManager,
    App *app, int groupIdx, int idx, bool signMessages, uint64_t maxBatchSize,
          uint64_t batchTimeoutMS, uint64_t EbatchSize, uint64_t EbatchTimeoutMS, bool primaryCoordinator, bool requestTx, int hotstuff_cpu, int numShards, Transport *transport);
  ~Replica();

  // Message handlers.
  void ReceiveMessage(const TransportAddress &remote, const std::string &type,
                      const std::string &data, void *meta_data);

  void ReceiveFromBFTSmart(const string &type, const string &data);

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
  // IndicusInterface hotstuff_interface;
  std::unordered_map<std::string, proto::PackedMessage> requests_dup;

  const transport::Configuration &config;
  KeyManager *keyManager;
  App *app;
  int groupIdx;
  int idx; // the replica index within the group
  int id; // unique replica id (across all shards)
  bool signMessages;
  uint64_t maxBatchSize;
  uint64_t batchTimeoutMS;
  uint64_t EbatchSize;
  uint64_t EbatchTimeoutMS;
  bool primaryCoordinator;
  bool requestTx;
  Transport *transport;
  int currentView;
  int nextSeqNum;
  int numShards;

  // members to reduce alloc
  proto::SignedMessage tmpsignedMessage;
  proto::Request recvrequest;
  proto::Preprepare recvpreprepare;
  proto::Prepare recvprepare;
  proto::Commit recvcommit;
  proto::BatchedRequest recvbatchedRequest;
  proto::GroupedSignedMessage recvgrouped;
  proto::Read recvrd;
  proto::RequestRequest recvrr;
  proto::ABRequest recvab;

  std::unordered_map<uint64_t, const TransportAddress*> clientCache; 

  std::unordered_map<uint64_t, std::string> sessionKeys;
  bool ValidateHMACedMessage(const proto::SignedMessage &signedMessage, std::string &data, std::string &type);
  void CreateHMACedMessage(const ::google::protobuf::Message &msg, proto::SignedMessage& signedMessage);

  Slots slots;

  bool batchTimerRunning;
  int batchTimerId;
  int nextBatchNum;
  // the map from 0..(N-1) to pending digests
  std::unordered_map<uint64_t, std::string> pendingBatchedDigests;
  void sendBatchedPreprepare();
  std::unordered_map<uint64_t, std::string> bStatNames;

  bool EbatchTimerRunning;
  int EbatchTimerId;
  std::vector<::google::protobuf::Message*> EpendingBatchedMessages;
  std::vector<std::string> EpendingBatchedDigs;
  void EsendBatchedPreprepare();
  std::unordered_map<uint64_t, std::string> EbStatNames;
  void sendEbatch();
  void sendEbatch_internal();
  void delegateEbatch(std::vector<::google::protobuf::Message*> EpendingBatchedMessages_,
     std::vector<std::string> EpendingBatchedDigs_);
  std::vector<proto::SignedMessage*> EsignedMessages;

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
  //std::unordered_map<std::string, TransportAddress*> replyAddrs;
  tbb::concurrent_unordered_map<std::string, TransportAddress*> replyAddrs;
  //std::mutex replyAddrsMutex;

  // tests to see if we are ready to send commit or executute the slot
  void testSlot(uint64_t seqnum, uint64_t viewnum, std::string digest, bool gotPrepare);

  void executeSlots();

  void executeSlots_internal();
  void executeSlots_internal_multi();

  void executeSlots_callback(std::vector<::google::protobuf::Message*> &replies, string batchDigest, string digest);

  std::mutex batchMutex;

  void handleMessage(const TransportAddress &remote, const string &type, const string &data);

  // map from seqnum to view num to
  std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::unordered_map<std::string, int>>> actionTimers;

  void startActionTimer(uint64_t seq_num, uint64_t viewnum, std::string digest);

  void cancelActionTimer(uint64_t seq_num, uint64_t viewnum, std::string digest);

  Stats* stats;

  bftsmartstore::BftSmartAgent* bftsmartagent;


};

} // namespace bftsmartstore
#endif
