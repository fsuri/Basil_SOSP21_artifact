// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
#ifndef _JANUS_CLIENT_H_
#define _JANUS_CLIENT_H_

#include "lib/assert.h"
#include "lib/message.h"
#include "lib/configuration.h"
#include "lib/udptransport.h"
#include "replication/ir/client.h"
#include "store/common/timestamp.h"
#include "store/common/truetime.h"
#include "store/common/frontend/client.h"
#include "store/common/frontend/bufferclient.h"

// TODO define these
#include "store/janusstore/shardclient.h"
#include "store/janusstore/janus-proto.pb.h"
// end TODO

#include <thread>

namespace janusstore {

class Client : public ::Client {
 public:
  Client(const std::string configPath, int nShards,
      int closestReplica, Transport *transport,
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

  // TODO these should probably be defined in server.h because client is no longer a coordinator
  /*
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
  };

  // Prepare function
  void Prepare(PendingRequest *req, uint32_t timeout);
  void PrepareCallback(uint64_t reqId, int status, Timestamp ts);
  void HandleAllPreparesReceived(PendingRequest *req);

  uint64_t lastReqId;
  std::unordered_map<uint64_t, PendingRequest *> pendingReqs;

  // Number of retries for current transaction.
  long retries;

  // Ongoing transaction ID.
  uint64_t t_id;

  // List of participants in the ongoing transaction.
  std::set<int> participants;

  // Buffering client for each shard.
  std::vector<BufferClient *> bclient;
 */

  // Unique ID for this client.
  uint64_t client_id;

  // Number of shards.
  uint64_t nshards;

  // Transport used by IR client proxies.
  Transport *transport;

};

} // namespace janusstore

#endif /* _JANUS_CLIENT_H_ */
