// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * store/indicus/shardclient.h:
 *   Single shard indicus transactional client interface.
 *
 * Copyright 2015 Irene Zhang <iyzhang@cs.washington.edu>
 *                Naveen Kr. Sharma <naveenks@cs.washington.edu>
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

#ifndef _INDICUS_SHARDCLIENT_H_
#define _INDICUS_SHARDCLIENT_H_

#include "bft_tapir/config.h"
#include "lib/assert.h"
#include "lib/configuration.h"
#include "lib/crypto.h"
#include "lib/message.h"
#include "lib/transport.h"
#include "replication/ir/client.h"
#include "store/common/timestamp.h"
#include "store/common/truetime.h"
#include "store/common/transaction.h"
#include "store/common/frontend/txnclient.h"
#include "store/common/common-proto.pb.h"
#include "store/indicusstore/indicus-proto.pb.h"

#include <map>
#include <string>

namespace indicusstore {

typedef std::function<void(int, const std::string &,
    const std::string &, Timestamp, const proto::Transaction &,
    bool)> read_callback;
typedef std::function<void(int, const std::string &)> read_timeout_callback;

class ShardClient : public TransportReceiver {
 public:
  /* Constructor needs path to shard config. */
  ShardClient(transport::Configuration *config, Transport *transport,
      uint64_t client_id, int shard, int closestReplica,
      uint64_t readQuorumSize, TrueTime &timeServer);
  virtual ~ShardClient();

  virtual void ReceiveMessage(const TransportAddress &remote,
      const std::string &type, const std::string &data,
      void *meta_data) override;

  // Begin a transaction.
  virtual void Begin(uint64_t id);

  // Get the value corresponding to key.
  virtual void Get(uint64_t id, const std::string &key, read_callback gcb,
      read_timeout_callback gtcb, uint32_t timeout);

  // Set the value for the given key.
  virtual void Put(uint64_t id, const std::string &key,
      const std::string &value, put_callback pcb, put_timeout_callback ptcb,
      uint32_t timeout);

  // Commit all Get(s) and Put(s) since Begin().
  virtual void Commit(uint64_t id, uint64_t timestamp, commit_callback ccb,
      commit_timeout_callback ctcb, uint32_t timeout);

  // Abort all Get(s) and Put(s) since Begin().
  virtual void Abort(uint64_t id, abort_callback acb,
      abort_timeout_callback atcb, uint32_t timeout);

  // Prepare the transaction.
  virtual void Prepare(uint64_t id, const Timestamp &timestamp,
      prepare_callback pcb, prepare_timeout_callback ptcb, uint32_t timeout);

 private:
  struct PendingQuorumGet {
    PendingQuorumGet(uint64_t reqId) : reqId(reqId),
        numReplies(0UL), numOKReplies(0UL), hasDep(false) { }
    ~PendingQuorumGet() { }
    uint64_t reqId;
    std::string key;
    Timestamp maxTs;
    std::string maxValue;
    uint64_t numReplies;
    uint64_t numOKReplies;
    proto::Transaction dep;
    bool hasDep;
    read_callback gcb;
    read_timeout_callback gtcb;
  };

  struct PendingPrepare {
    PendingPrepare(uint64_t reqId) : reqId(reqId),
        requestTimeout(nullptr) { }
    ~PendingPrepare() {
      if (requestTimeout != nullptr) {
        delete requestTimeout;
      }
    }
    uint64_t reqId;
    Timestamp ts;
    proto::Transaction txn;
    Timeout *requestTimeout;
    prepare_callback pcb;
    prepare_timeout_callback ptcb;

  };
  struct PendingCommit {
    PendingCommit(uint64_t reqId) : reqId(reqId),
        requestTimeout(nullptr) { }
    ~PendingCommit() {
      if (requestTimeout != nullptr) {
        delete requestTimeout;
      }
    }
    uint64_t reqId;
    uint64_t ts;
    proto::Transaction txn;
    Timeout *requestTimeout;
    commit_callback ccb;
    commit_timeout_callback ctcb;
  };
  struct PendingAbort {
    PendingAbort(uint64_t reqId) : reqId(reqId),
        requestTimeout(nullptr) { }
    ~PendingAbort() {
      if (requestTimeout != nullptr) {
        delete requestTimeout;
      }
    }
    uint64_t reqId;
    proto::Transaction txn;
    Timeout *requestTimeout;
    abort_callback acb;
    abort_timeout_callback atcb;
  };

  bool BufferGet(const std::string &key, read_callback rcb);

  /* Timeout for Get requests, which only go to one replica. */
  void GetTimeout(uint64_t reqId);

  /* Callbacks for hearing back from a shard for an operation. */
  void HandleReadReply(const proto::ReadReply &readReply);
  void HandlePhase1Reply(const proto::Phase1Reply &phase1Reply);
  void HandlePhase2Reply(const proto::Phase2Reply &phase2Reply);
  bool CommitCallback(uint64_t reqId, const std::string &,
      const std::string &);
  bool AbortCallback(uint64_t reqId, const std::string &,
      const std::string &);

  bool ValidateWriteProof(const proto::WriteProof &proof, const std::string &key,
      const std::string &val, const Timestamp &timestamp);

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
  uint64_t readQuorumSize;
  TrueTime &timeServer;
  bool signedMessages;
  bool validateProofs;
  bft_tapir::NodeConfig *cryptoConfig;

  uint64_t lastReqId;
  proto::Transaction txn;
  std::unordered_map<uint64_t, PendingQuorumGet *> pendingGets;
  std::unordered_map<uint64_t, PendingPrepare *> pendingPrepares;
  std::unordered_map<uint64_t, PendingCommit *> pendingCommits;
  std::unordered_map<uint64_t, PendingAbort *> pendingAborts;
};

} // namespace indicusstore

#endif /* _INDICUS_SHARDCLIENT_H_ */
