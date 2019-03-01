#ifndef _MORTY_SHARDCLIENT_H_
#define _MORTY_SHARDCLIENT_H_

#include "lib/assert.h"
#include "lib/configuration.h"
#include "lib/message.h"
#include "lib/transport.h"
#include "replication/ir/client.h"
#include "store/common/timestamp.h"
#include "store/common/transaction.h"
#include "store/common/frontend/asyncclient.h"
#include "store/mortystore/morty-proto.pb.h"

#include <map>
#include <string>

namespace mortystore {

class ShardClient {
 public:
  /* Constructor needs path to shard config. */
  ShardClient(const std::string &configPath, Transport *transport,
      uint64_t client_id, int shard, int closestReplica);
  ~ShardClient();

  // Overriding from TxnClient
  void Begin(uint64_t id);
  void Get(uint64_t id, const std::string &key, get_callback cb);
  void Put(uint64_t id, const std::string &key, const std::string &value,
	    put_callback cb);
  void Prepare(uint64_t id, const Transaction &txn);
  void Commit(uint64_t id, const Transaction &txn, commit_callback cb);
  void Abort(uint64_t id, const Transaction &txn, abort_callback cb);

 private:
  /* Timeout for Get requests, which only go to one replica. */
  void GetTimeout();
  void PutTimeout();
  void PrepareTimeout();
  void CommitTimeout();
  void AbortTimeout();

  /* Callbacks for hearing back from a shard for an operation. */
  void GetCallback(get_callback cb, const std::string &req,
      const std::string &reply);
  void PutCallback(put_callback cb, const std::string &req,
      const std::string &reply);
  void PrepareCallback(const std::string &req,
      const std::string &reply);
  void CommitCallback(commit_callback cb, const std::string &req,
      const std::string &reply);
  void AbortCallback(abort_callback cb, const std::string &req,
      const std::string &reply);

  /* Helper Functions for starting and finishing requests */
  void StartRequest();
  void WaitForResponse();
  void FinishRequest(const std::string &reply_str);
  void FinishRequest();
  int SendGet(const std::string &request_str);

  uint64_t client_id; // Unique ID for this client.
  Transport *transport; // Transport layer.
  transport::Configuration *config;
  int shard; // which shard this client accesses
  int replica; // which replica to use for reads
  replication::ir::IRClient *client; // Client proxy.
};

} // namespace mortystore

#endif /* _MORTY_SHARDCLIENT_H_ */
