#ifndef _PBFT_CLIENT_H_
#define _PBFT_CLIENT_H_

#include "lib/assert.h"
#include "lib/keymanager.h"
#include "lib/message.h"
#include "lib/configuration.h"
#include "lib/udptransport.h"
#include "replication/ir/client.h"
#include "store/common/timestamp.h"
#include "store/common/truetime.h"
#include "store/common/frontend/client.h"
#include "store/pbftstore/pbft-proto.pb.h"

#include <thread>
#include <set>

namespace pbftstore {

class Client : public ::Client {
 public:
  Client(transport::Configuration *config, int nShards, int nGroups,
      int closestReplica, Transport *transport, partitioner part,
      bool syncCommit, uint64_t readQuorumSize, bool signedMessages,
      bool validateProofs, KeyManager *keyManager,
      TrueTime timeserver = TrueTime(0,0));
  virtual ~Client();

  // Begin a transaction.
  virtual void Begin();

  // Get the value corresponding to key.
  virtual void Get(const std::string &key, get_callback gcb,
      get_timeout_callback gtcb, uint32_t timeout = GET_TIMEOUT);

  // Set the value for the given key.
  virtual void Put(const std::string &key, const std::string &value,
      put_callback pcb, put_timeout_callback ptcb,
      uint32_t timeout = PUT_TIMEOUT);

  // Commit all Get(s) and Put(s) since Begin().
  virtual void Commit(commit_callback ccb, commit_timeout_callback ctcb,
      uint32_t timeout);

  // Abort all Get(s) and Put(s) since Begin().
  virtual void Abort(abort_callback acb, abort_timeout_callback atcb,
      uint32_t timeout);

  virtual std::vector<int> Stats();

 private:
  struct PendingRequest {
    PendingRequest(uint64_t id) : id(id), outstandingConfirms(0),
        commitTries(0), decision(proto::COMMIT), prepareTimestamp(nullptr) {
    }

    ~PendingRequest() {}

    commit_callback ccb;
    commit_timeout_callback ctcb;
    uint64_t id;
    int outstandingConfirms;
    int commitTries;
    proto::CommitDecision decision;
    Timestamp *prepareTimestamp;
    bool callbackInvoked;
    std::map<int, std::vector<proto::Phase1Reply>> phase1RepliesGrouped;
  };

  // Prepare function
  void Phase1(PendingRequest *req, uint32_t timeout);
  void Phase1Callback(uint64_t reqId, int group, proto::CommitDecision decision,
      bool fast, const std::vector<proto::Phase1Reply> &phase1Replies,
      const std::vector<proto::SignedMessage> &signedPhase1Replies);
  void Phase1TimeoutCallback(uint64_t reqId, int status);
  void HandleAllPhase1Received(PendingRequest *req);

  void Phase2(PendingRequest *req, uint32_t timeout);
  void Phase2Callback(uint64_t reqId,
      const std::vector<proto::Phase2Reply> &phase2Replies,
      const std::vector<proto::SignedMessage> &signedPhase2Replies);
  void Phase2TimeoutCallback(uint64_t reqId, int status);

  void Writeback(PendingRequest *req, uint32_t timeout);

  /* Configuration State */
  transport::Configuration *config;
  // Unique ID for this client.
  uint64_t client_id;
  // Number of shards.
  uint64_t nshards;
  // Number of replica groups.
  uint64_t ngroups;
  // Transport used by shard clients.
  Transport *transport;
  partitioner part;
  bool syncCommit;
  uint64_t readQuorumSize;
  bool signedMessages;
  bool validateProofs;
  KeyManager *keyManager;
  // TrueTime server.
  TrueTime timeServer;


  /* Transaction Execution State */
  // Ongoing transaction ID.
  uint64_t client_seq_num;
  // Read timestamp for transaction.
  Timestamp rts;
  // Last request ID.
  uint64_t lastReqId;
  // Number of retries for current transaction.
  long retries;
  // Current transaction.
  proto::Transaction txn;
  // Outstanding requests.
  std::unordered_map<uint64_t, PendingRequest *> pendingReqs;

  /* Debug State */
  std::unordered_map<std::string, uint32_t> statInts;
};

} // namespace indicusstore

#endif /* _INDICUS_CLIENT_H_ */
