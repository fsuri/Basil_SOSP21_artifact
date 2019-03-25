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
#include "store/common/frontend/async_client.h"

#include <thread>
#include <set>

namespace mortystore {

struct Branch {
  uint64_t id;
  std::set<int> participants;
  size_t opCount;
  std::map<std::string, std::string> readValues;
};

class Client : public ::AsyncClient {
 public:
  Client(const std::string configPath, int nShards, int closestReplica,
      Transport *transport);
  virtual ~Client();

  virtual void Execute(AsyncTransaction *txn, execute_callback ecb);

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
  };

  void ExecuteNextOperation(AsyncTransaction *txn, Branch *branch);

  void Get(Branch *branch, const std::string &key);
  void Put(Branch *branch, const std::string &key, const std::string &value);
  void Commit(Branch *branch);
  void Abort(Branch *branch);

  void GetCallback(Branch *branch, int status, const std::string &key,
      const std::string &val);
  void PutCallback(Branch *branch, int status, const std::string &key,
      const std::string &val);

  void GetTimeout(Branch *branch, int status, const std::string &key);
  void PutTimeout(Branch *branc, int status, const std::string &key,
      const std::string &value);

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
  AsyncTransaction *currTxn;
  execute_callback currEcb;
};

} // namespace mortystore

#endif /* _MORTY_CLIENT_H_ */
