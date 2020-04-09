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
   uint64_t client_id;
  /* Configuration State */
  transport::Configuration *config;
  // Number of shards.
  uint64_t nshards;
  // Number of replica groups.
  uint64_t ngroups;
  // Transport used by shard clients.
  Transport *transport;
  // Client for each shard
  std::vector<ShardClient *> bclient;
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
  std::unordered_map<uint64_t, PendingRead *> pendingReads;
  //
  std::unordered_map<uint64_t, ShardCommitMessage* > gatheredCommits;

  /* Debug State */
  std::unordered_map<std::string, uint32_t> statInts;
};

} // namespace indicusstore

#endif /* _INDICUS_CLIENT_H_ */
