#ifndef _TAPIR_ASYNC_CLIENT_H_
#define _TAPIR_ASYNC_CLIENT_H_

#include "lib/assert.h"
#include "lib/message.h"
#include "lib/configuration.h"
#include "lib/udptransport.h"
#include "replication/ir/client.h"
#include "store/common/timestamp.h"
#include "store/common/truetime.h"
#include "store/common/frontend/asyncclient.h"
#include "store/common/frontend/bufferclient.h"
#include "store/tapirstore/shardclient.h"
#include "store/tapirstore/tapir-proto.pb.h"
#include "store/common/frontend/client.h"

#include <thread>
#include <list>

namespace tapirstore {

class AsyncClient : public ::AsyncClient {
 public:
  AsyncClient(const std::string configPath, int nShards, int closestReplica,
      TrueTime timeserver = TrueTime(0,0));
  virtual ~AsyncClient();

  // Overriding functions from ::Client.
  void Begin();
  void Get(const std::string &key, get_callback cb);
  void Put(const std::string &key, const std::string &value, put_callback cb);
  void Commit(commit_callback cb);
  void Abort(abort_callback cb);
  std::vector<int> Stats();

 private:
  // Unique ID for this client.
  uint64_t client_id;

  // Ongoing transaction ID.
  uint64_t t_id;

  // Number of shards.
  uint64_t nshards;

  // Number of retries for current transaction.
  long retries;

  // List of participants in the ongoing transaction.
  std::set<int> participants;

  // Transport used by IR client proxies.
  UDPTransport transport;
    
  // Thread running the transport event loop.
  std::thread *clientTransport;

  // Buffering client for each shard.
  std::vector<BufferClient *> bclient;

  // TrueTime server.
  TrueTime timeServer;

  // Prepare function
  void Prepare(Timestamp &timestamp, commit_callback cb);

  // Runs the transport event loop.
  void run_client();

  int outstandingPrepares;
  int commitTries;
  std::list<Promise *> preparePromises;
  uint64_t maxRepliedTs;
  int prepareStatus;
  Timestamp *prepareTimestamp;

  void PrepareCallback(commit_callback cb, Promise *p);
  void ResetPrepare();
};

} // namespace tapirstore

#endif /* _TAPIR_ASYNC_CLIENT_H_ */
