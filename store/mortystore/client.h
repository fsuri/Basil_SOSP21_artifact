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
#include "store/mortystore/shardclient.h"
#include "store/mortystore/morty-proto.pb.h"

#include <thread>
#include <set>

namespace mortystore {

class Client : public ::Client {
 public:
  Client(const std::string configPath, int nShards, int closestReplica,
      Transport *transport);
  virtual ~Client();

  // Begin a transaction.
  virtual void Begin();

  // Get the value corresponding to key.
  virtual void Get(const std::string &key, get_callback gcb,
      get_timeout_callback gtcb, uint32_t timeout);

  // Set the value for the given key.
  virtual void Put(const std::string &key, const std::string &value,
      put_callback pcb, put_timeout_callback ptcb,
      uint32_t timeout);

  // Commit all Get(s) and Put(s) since Begin().
  virtual void Commit(commit_callback ccb, commit_timeout_callback ctcb,
      uint32_t timeout);
  
  // Abort all Get(s) and Put(s) since Begin().
  virtual void Abort(abort_callback acb, abort_timeout_callback atcb,
      uint32_t timeout);

  virtual std::vector<int> Stats();

  void Execute(AsyncTransaction *txn);

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

    commit_callback ccb;
    commit_timeout_callback ctcb;
    uint64_t id;
    int outstandingPrepares;
    int commitTries;
    uint64_t maxRepliedTs;
    int prepareStatus;
    Timestamp *prepareTimestamp;
    bool callbackInvoked;
  };

  // Prepare function
  void Prepare(PendingRequest *req, uint32_t timeout);
  void PrepareCallback(uint64_t reqId, int status, Timestamp ts);
  void HandleAllPreparesReceived(PendingRequest *req);

  // Unique ID for this client.
  uint64_t client_id;

  // Ongoing transaction ID.
  uint64_t t_id;

  // Number of shards.
  uint64_t nshards;

  // Transport used by client proxies.
  Transport *transport;
  
  // ShardClient for each shard.
  std::vector<ShardClient *> sclient;

  uint64_t lastReqId;
  std::unordered_map<uint64_t, PendingRequest *> pendingReqs;
};

} // namespace mortystore

#endif /* _MORTY_CLIENT_H_ */
