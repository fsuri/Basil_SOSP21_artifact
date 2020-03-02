#ifndef _MORTY_CLIENT_H_
#define _MORTY_CLIENT_H_

#include "lib/assert.h"
#include "lib/message.h"
#include "lib/configuration.h"
#include "lib/udptransport.h"
#include "replication/ir/client.h"
#include "store/common/timestamp.h"
#include "store/common/truetime.h"
#include "store/common/frontend/client.h"
#include "store/common/frontend/bufferclient.h"
#include "store/mortystore/morty-proto.pb.h"
#include "store/common/frontend/async_client.h"
#include "store/mortystore/shardclient.h"
#include "store/mortystore/common.h"
#include "lib/latency.h"

#include <thread>
#include <set>

namespace mortystore {

struct ClientBranch {
  size_t opCount;
  std::map<std::string, std::string> readValues;
};

class Client : public ::AsyncClient {
 public:
  Client(const std::string configPath, uint64_t client_id, int nShards, int nGroups,
      int closestReplica, Transport *transport, partitioner part);
  virtual ~Client();

  virtual void Execute(AsyncTransaction *txn, execute_callback ecb);
  
 private:
  struct PendingRequest {
    PendingRequest(uint64_t id) : id(id), sentPrepares(0UL),
        prepareResponses(0UL), commitTries(0), maxRepliedTs(0UL),
        prepareStatus(REPLY_OK), prepareTimestamp(nullptr) {
    }

    ~PendingRequest() {
      if (prepareTimestamp != nullptr) {
        delete prepareTimestamp;
      }
    }

    proto::Transaction protoTxn;
    AsyncTransaction *txn;
    execute_callback ecb;
    uint64_t id;
    uint64_t sentPrepares;
    uint64_t prepareResponses;
    int commitTries;
    uint64_t maxRepliedTs;
    int prepareStatus;
    Timestamp *prepareTimestamp;
    bool callbackInvoked;
    std::unordered_map<proto::Branch, uint64_t, BranchHasher, BranchComparer> prepareOKs; 
    std::unordered_map<proto::Branch, std::vector<proto::PrepareKO>, BranchHasher, BranchComparer> prepareKOes;
  };

  void ExecuteNextOperation(PendingRequest *req, proto::Branch &branch);
  ClientBranch GetClientBranch(const proto::Branch &branch);

  friend class ShardClient;
  void HandleReadReply(const TransportAddress &remote, const proto::ReadReply &msg, uint64_t shard);
  void HandleWriteReply(const TransportAddress &remote, const proto::WriteReply &msg, uint64_t shard);
  void HandlePrepareOK(const TransportAddress &remote, const proto::PrepareOK &msg, uint64_t shard);
  void HandleCommitReply(const TransportAddress &remote, const proto::CommitReply &msg, uint64_t shard);
  void HandlePrepareKO(const TransportAddress &remote, const proto::PrepareKO &msg, uint64_t shard);

  void Get(PendingRequest *req, proto::Branch &branch, const std::string &key);
  void Put(proto::Branch &branch, const std::string &key,
      const std::string &value);
  void Commit(PendingRequest *req, const proto::Branch &branch);
  void Abort(const proto::Branch &branch);

  void ProcessPrepareKOs(PendingRequest *req, const proto::Branch &branch);

  void RecordBranch(const proto::Branch &branch);

  // Unique ID for this client.
  uint64_t client_id;

  // Ongoing transaction ID.
  uint64_t t_id;

  // Number of shards.
  uint64_t nshards;
  uint64_t ngroups;

  // Transport used by client proxies.
  Transport *transport;

  partitioner part;
  
  uint64_t lastReqId;
  std::unordered_map<uint64_t, PendingRequest *> pendingReqs;
  std::vector<ShardClient *> sclients;
  uint64_t prepareBranchIds;
  transport::Configuration *config;
  Latency_t opLat;
  std::unordered_set<proto::Branch, BranchHasher, BranchComparer> sent_branches;
};

} // namespace mortystore

#endif /* _MORTY_CLIENT_H_ */
