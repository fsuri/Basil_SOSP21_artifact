#ifndef _MORTY_SHARDCLIENT_H_
#define _MORTY_SHARDCLIENT_H_

#include "lib/assert.h"
#include "lib/configuration.h"
#include "lib/message.h"
#include "lib/transport.h"
#include "replication/ir/client.h"
#include "store/common/timestamp.h"
#include "store/common/transaction.h"
#include "store/mortystore/morty-proto.pb.h"
#include "store/common/frontend/client.h"
#include "store/common/frontend/txnclient.h"
#include "store/mortystore/replica_client.h"

#include <map>
#include <string>

namespace mortystore {

class ShardClient : public TxnClient {
 public:
  /* Constructor needs path to shard config. */
  ShardClient(const std::string &configPath, Transport *transport,
      uint64_t client_id, int shard, int closestReplica);
  virtual ~ShardClient();

  // Begin a transaction.
  virtual void Begin(uint64_t id) override;

  // Get the value corresponding to key.
  virtual void Get(uint64_t id, const std::string &key, get_callback gcb,
      get_timeout_callback gtcb, uint32_t timeout) override;
  virtual void Get(uint64_t id, const std::string &key,
      const Timestamp &timestamp, get_callback gcb, get_timeout_callback gtcb,
      uint32_t timeout) override;

  // Set the value for the given key.
  virtual void Put(uint64_t id, const std::string &key,
      const std::string &value, put_callback pcb, put_timeout_callback ptcb,
      uint32_t timeout) override;

  // Commit all Get(s) and Put(s) since Begin().
  virtual void Commit(uint64_t id, const Transaction & txn,
      uint64_t timestamp, commit_callback ccb, commit_timeout_callback ctcb,
      uint32_t timeout) override;
  
  // Abort all Get(s) and Put(s) since Begin().
  virtual void Abort(uint64_t id, const Transaction &txn,
      abort_callback acb, abort_timeout_callback atcb,
      uint32_t timeout) override;

  // Prepare the transaction.
  virtual void Prepare(uint64_t id, const Transaction &txn,
      const Timestamp &timestamp, prepare_callback pcb,
      prepare_timeout_callback ptcb, uint32_t timeout) override;

  void MarkComplete(uint64_t tid);

 private:
  struct PendingPrepare : public TxnClient::PendingPrepare {
    PendingPrepare(uint64_t reqId) : TxnClient::PendingPrepare(reqId),
        requestTimeout(nullptr) { }
    ~PendingPrepare() {
      if (requestTimeout != nullptr) {
        delete requestTimeout;
      }
    }
    Timeout *requestTimeout;
  };
  struct PendingCommit : public TxnClient::PendingCommit {
    PendingCommit(uint64_t reqId) : TxnClient::PendingCommit(reqId),
        requestTimeout(nullptr) { }
    ~PendingCommit() {
      if (requestTimeout != nullptr) {
        delete requestTimeout;
      }
    }
    Timeout *requestTimeout;
  };
  struct PendingAbort : public TxnClient::PendingAbort {
    PendingAbort(uint64_t reqId) : TxnClient::PendingAbort(reqId),
        requestTimeout(nullptr) { }
    ~PendingAbort() {
      if (requestTimeout != nullptr) {
        delete requestTimeout;
      }
    }
    Timeout *requestTimeout;
  };

  void AddOutstanding(uint64_t tid, uint64_t reqId);
  
  void GetCallback(uint64_t reqId, const std::string &request_str,
    const std::string &reply_str);
  void PutCallback(uint64_t reqId, const std::string &request_str,
    const std::string &reply_str);
  void PrepareCallback(const std::string &request_str,
    const std::string &reply_str);
  void CommitCallback(const std::string &request_str,
    const std::string &reply_str);
  void AbortCallback(const std::string &request_str,
    const std::string &reply_str);
  
  void GetTimeout(uint64_t reqId);
  void PutTimeout(uint64_t reqId);
  void PrepareTimeout();
  void CommitTimeout();
  void AbortTimeout();

  uint64_t client_id; // Unique ID for this client.
  Transport *transport; // Transport layer.
  transport::Configuration *config;
  int shard; // which shard this client accesses
  int replica; // which replica to use for gets/puts

  ReplicaClient *client; // Client proxy.

  uint64_t lastReqId;
  std::unordered_map<uint64_t, PendingGet *> pendingGets;
  std::unordered_map<uint64_t, PendingPut *> pendingPuts;
  std::unordered_map<uint64_t, PendingPrepare *> pendingPrepares;
  std::unordered_map<uint64_t, PendingCommit *> pendingCommits;
  std::unordered_map<uint64_t, PendingAbort *> pendingAborts;
  std::unordered_map<uint64_t, std::vector<uint64_t>> outstanding;

};

} // namespace mortystore

#endif /* _MORTY_SHARDCLIENT_H_ */
