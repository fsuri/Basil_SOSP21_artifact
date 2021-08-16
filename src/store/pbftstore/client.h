/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fs435@cornell.edu>
 *                Matthew Burke <matthelb@cs.cornell.edu>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************************/
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
#include "store/pbftstore/shardclient.h"

#include <unordered_map>

namespace pbftstore {

class Client : public ::Client {
 public:
  Client(const transport::Configuration& config, int nGroups, int nShards,
      Transport *transport, Partitioner *part,
      uint64_t readQuorumSize, bool signMessages,
      bool validateProofs, KeyManager *keyManager,
      bool order_commit = false, bool validate_abort = false,
      TrueTime timeserver = TrueTime(0,0));
  ~Client();

  // Begin a transaction.
  virtual void Begin(begin_callback bcb, begin_timeout_callback btcb,
      uint32_t timeout) override;

  // Get the value corresponding to key.
  virtual void Get(const std::string &key, get_callback gcb,
      get_timeout_callback gtcb, uint32_t timeout) override;

  // Set the value for the given key.
  virtual void Put(const std::string &key, const std::string &value,
      put_callback pcb, put_timeout_callback ptcb,
      uint32_t timeout) override;

  // Commit all Get(s) and Put(s) since Begin().
  virtual void Commit(commit_callback ccb, commit_timeout_callback ctcb,
      uint32_t timeout) override;

  // Abort all Get(s) and Put(s) since Begin().
  virtual void Abort(abort_callback acb, abort_timeout_callback atcb,
      uint32_t timeout) override;

 private:
  uint64_t client_id;
  /* Configuration State */
  transport::Configuration config;
  // Number of replica groups.
  uint64_t nshards;
  // Number of replica groups.
  uint64_t ngroups;
  // Transport used by shard clients.
  Transport *transport;
  // Client for each shard
  std::vector<ShardClient *> bclient;
  Partitioner *part;
  uint64_t readQuorumSize;
  bool signMessages;
  bool validateProofs;
  KeyManager *keyManager;
  // TrueTime server.
  TrueTime timeServer;
  int client_seq_num;

  //addtional knobs: 1) order commit, 2) validate abort
  bool order_commit = false;
  bool validate_abort = false;

  struct PendingPrepare {
    proto::Transaction txn;
    // collected decisions from each shard
    std::unordered_map<uint64_t, proto::TransactionDecision> shardDecisions;
    std::unordered_map<uint64_t, proto::GroupedSignedMessage> signedShardDecisions;

    commit_callback ccb;
    commit_timeout_callback ctcb;
    uint32_t timeout;
  };

  struct PendingWriteback {
    proto::Transaction txn;
    // set of replicas we got a writeback from
    std::unordered_set<uint64_t> writebackAcks;

    commit_callback ccb;
  };

  void HandleSignedPrepareReply(std::string digest, uint64_t shard_id, int status, const proto::GroupedSignedMessage& gsm);

  void HandlePrepareReply(std::string digest, uint64_t shard_id, int status, const proto::TransactionDecision& txndec);

  void HandleWritebackReply(std::string digest, uint64_t shard_id, int status);

  // Current transaction.
  proto::Transaction currentTxn;

  // map from txn digest to pending prepare state
  std::unordered_map<std::string, PendingPrepare> pendingPrepares;

  // map from txn digest to pending writeback state
  std::unordered_map<std::string, PendingWriteback> pendingWritebacks;

  /* Debug State */
  std::unordered_map<std::string, uint32_t> statInts;

  void WriteBackSigned(const proto::ShardSignedDecisions& dec, const proto::Transaction& txn, std::string digest);

  void WriteBackSigned(const proto::ShardSignedDecisions& dec, const proto::Transaction& txn,
    commit_callback ccb, commit_timeout_callback ctcb, uint32_t timeout);

  void WriteBack(const proto::ShardDecisions& dec, const proto::Transaction& txn,
    commit_callback ccb, commit_timeout_callback ctcb, uint32_t timeout);

  void AbortTxnSigned(const proto::ShardSignedDecisions& dec, const proto::Transaction& txn, std::string& digest);

  void AbortTxn(const proto::Transaction& txn);

  bool IsParticipant(int g);
};

} // namespace pbftstore

#endif /* _PBFT_CLIENT_H_ */
