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

#include "lib/keymanager.h"
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
#include <vector>

namespace indicusstore {

typedef std::function<void(int, const std::string &,
    const std::string &, const Timestamp &, const proto::Dependency &,
    bool, bool)> read_callback;
typedef std::function<void(int, const std::string &)> read_timeout_callback;

typedef std::function<void(proto::CommitDecision, bool,
    const std::vector<proto::Phase1Reply> &,
    const std::vector<proto::SignedMessage> &)> phase1_callback;
typedef std::function<void(int)> phase1_timeout_callback;

typedef std::function<void(
    const std::vector<proto::Phase2Reply> &,
    const std::vector<proto::SignedMessage> &)> phase2_callback;
typedef std::function<void(int)> phase2_timeout_callback;

typedef std::function<void()> writeback_callback;
typedef std::function<void(int)> writeback_timeout_callback;

class ShardClient : public TransportReceiver {
 public:
  ShardClient(transport::Configuration *config, Transport *transport,
      uint64_t client_id, int group, const std::vector<int> &closestReplicas,
      bool signedMessages, bool validateProofs,
      KeyManager *keyManager, TrueTime &timeServer);
  virtual ~ShardClient();

  virtual void ReceiveMessage(const TransportAddress &remote,
      const std::string &type, const std::string &data,
      void *meta_data) override;

  // Begin a transaction.
  virtual void Begin(uint64_t id);

  // Get the value corresponding to key.
  virtual void Get(uint64_t id, const std::string &key, const TimestampMessage &ts,
      uint64_t rqs, read_callback gcb, read_timeout_callback gtcb,
      uint32_t timeout);

  // Set the value for the given key.
  virtual void Put(uint64_t id, const std::string &key,
      const std::string &value, put_callback pcb, put_timeout_callback ptcb,
      uint32_t timeout);

  virtual void Phase1(uint64_t id, const proto::Transaction &transaction,
      phase1_callback pcb, phase1_timeout_callback ptcb, uint32_t timeout);
  virtual void Phase2(uint64_t id, const proto::Transaction &transaction,
      const std::map<int, std::vector<proto::Phase1Reply>> &groupedPhase1Replies,
      const std::map<int, std::vector<proto::SignedMessage>> &groupedSignedPhase1Replies,
      proto::CommitDecision decision, phase2_callback pcb,
      phase2_timeout_callback ptcb, uint32_t timeout);
  virtual void Writeback(uint64_t id, const proto::Transaction &transaction,
      const std::string &txnDigest,
      proto::CommitDecision decision, const proto::CommittedProof &proof,
      writeback_callback wcb, writeback_timeout_callback wtcb, uint32_t timeout);
  
  virtual void Abort(uint64_t id, const TimestampMessage &ts);
 private:
  struct PendingQuorumGet {
    PendingQuorumGet(uint64_t reqId) : reqId(reqId),
        numReplies(0UL), numOKReplies(0UL), hasDep(false) { }
    ~PendingQuorumGet() { }
    uint64_t reqId;
    std::string key;
    Timestamp rts;
    uint64_t rqs;
    Timestamp maxTs;
    std::string maxValue;
    uint64_t numReplies;
    uint64_t numOKReplies;
    std::map<Timestamp, std::pair<proto::PreparedWrite, uint64_t>> prepared;
    std::map<Timestamp, std::vector<proto::SignedMessage>> signedPrepared;
    proto::Dependency dep;
    bool hasDep;
    read_callback gcb;
    read_timeout_callback gtcb;
  };

  struct PendingPhase1 {
    PendingPhase1(uint64_t reqId) : reqId(reqId),
        requestTimeout(nullptr), decisionTimeout(nullptr),
        decisionTimeoutStarted(false) { }
    ~PendingPhase1() {
      if (requestTimeout != nullptr) {
        delete requestTimeout;
      }
      if (decisionTimeout != nullptr) {
        delete decisionTimeout;
      }
    }
    uint64_t reqId;
    Timeout *requestTimeout;
    Timeout *decisionTimeout;
    bool decisionTimeoutStarted;
    std::vector<proto::Phase1Reply> phase1Replies;
    std::vector<proto::SignedMessage> signedPhase1Replies;
    phase1_callback pcb;
    phase1_timeout_callback ptcb;
    proto::Transaction transaction;
  };

  struct PendingPhase2 {
    PendingPhase2(uint64_t reqId, proto::CommitDecision decision) : reqId(reqId),
        decision(decision), requestTimeout(nullptr) { }
    ~PendingPhase2() {
      if (requestTimeout != nullptr) {
        delete requestTimeout;
      }
    }
    uint64_t reqId;
    proto::CommitDecision decision;
    Timeout *requestTimeout;

    std::vector<proto::Phase2Reply> phase2Replies;
    std::vector<proto::SignedMessage> signedPhase2Replies;
    uint64_t matchingReplies;
    phase2_callback pcb;
    phase2_timeout_callback ptcb;
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
    writeback_callback ccb;
    writeback_timeout_callback ctcb;
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
  void HandlePhase1Reply(const proto::Phase1Reply &phase1Reply,
      const proto::SignedMessage &signedPhase1Reply);
  void HandlePhase2Reply(const proto::Phase2Reply &phase2Reply,
      const proto::SignedMessage &signedPhase2Reply);
  bool CommitCallback(uint64_t reqId, const std::string &,
      const std::string &);
  bool AbortCallback(uint64_t reqId, const std::string &,
      const std::string &);

  inline uint64_t QuorumSize() const { return 4 * config->f + 1; } 
  void Phase1Decision(uint64_t reqId);
  void Phase1Decision(
      std::unordered_map<uint64_t, PendingPhase1 *>::iterator itr);

  /* Helper Functions for starting and finishing requests */
  void StartRequest();
  void WaitForResponse();
  void FinishRequest(const std::string &reply_str);
  void FinishRequest();
  int SendGet(const std::string &request_str);

  uint64_t client_id; // Unique ID for this client.
  Transport *transport; // Transport layer.
  transport::Configuration *config;
  int group; // which shard this client accesses
  int replica; // which replica to use for reads
  TrueTime &timeServer;
  bool signedMessages;
  bool validateProofs;
  KeyManager *keyManager;
  uint64_t phase1DecisionTimeout;
  std::vector<int> closestReplicas;

  uint64_t lastReqId;
  proto::Transaction txn;
  std::map<std::string, std::string> readValues;

  std::unordered_map<uint64_t, PendingQuorumGet *> pendingGets;
  std::unordered_map<uint64_t, PendingPhase1 *> pendingPhase1s;
  std::unordered_map<uint64_t, PendingPhase2 *> pendingPhase2s;
  std::unordered_map<uint64_t, PendingCommit *> pendingCommits;
  std::unordered_map<uint64_t, PendingAbort *> pendingAborts;
};

} // namespace indicusstore

#endif /* _INDICUS_SHARDCLIENT_H_ */
