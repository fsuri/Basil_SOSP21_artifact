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

#include <thread>
#include <set>

namespace mortystore {

struct ClientBranch {
  size_t opCount;
  std::map<std::string, std::string> readValues;
};

class Client : public ::AsyncClient, public TransportReceiver {
 public:
  Client(const std::string configPath, int nShards, int nGroups,
      int closestReplica, Transport *transport, partitioner part);
  virtual ~Client();

  virtual void Execute(AsyncTransaction *txn, execute_callback ecb);
  
  virtual void ReceiveMessage(const TransportAddress &remote,
      const std::string &type, const std::string &data);

 private:
  struct PendingRequest {
    PendingRequest(uint64_t id) : id(id), outstandingPrepares(0), commitTries(0),
        maxRepliedTs(0UL), prepareStatus(REPLY_OK), prepareTimestamp(nullptr) {
    }

    ~PendingRequest() {
      if (prepareTimestamp != nullptr) {
        delete prepareTimestamp;
      }
    }

    proto::Transaction protoTxn;
    AsyncTransaction *txn;
    execute_callback ecb;
    commit_callback ccb;
    commit_timeout_callback ctcb;
    uint64_t id;
    int outstandingPrepares;
    int commitTries;
    uint64_t maxRepliedTs;
    int prepareStatus;
    Timestamp *prepareTimestamp;
    bool callbackInvoked;
    std::unordered_map<uint64_t, uint64_t> prepareOKs; 
    std::unordered_map<uint64_t, std::vector<proto::PrepareKO>> prepareKOes;
  };

  void ExecuteNextOperation(PendingRequest *req, proto::Branch &branch);
  ClientBranch GetClientBranch(const proto::Branch &branch);
  void ValueOnBranch(const proto::Branch *branch, const std::string &key,
      std::string &val);
  bool ValueInTransaction(const proto::Transaction &txn, const std::string &key,
      std::string &val);

  void HandleReadReply(const TransportAddress &remote, const proto::ReadReply &msg);
  void HandleWriteReply(const TransportAddress &remote, const proto::WriteReply &msg);
  void HandlePrepareOK(const TransportAddress &remote, const proto::PrepareOK &msg);
  void HandleCommitReply(const TransportAddress &remote, const proto::CommitReply &msg);
  void HandlePrepareKO(const TransportAddress &remote, const proto::PrepareKO &msg);

  void Get(proto::Branch &branch, const std::string &key);
  void Put(proto::Branch &branch, const std::string &key,
      const std::string &value);
  void Commit(const proto::Branch &branch);
  void Abort(const proto::Branch &branch);

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

};

} // namespace mortystore

#endif /* _MORTY_CLIENT_H_ */
