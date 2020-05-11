// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * store/indicusstore/client.h:
 *   Indicus client interface.
 *
 * Copyright 2015 Irene Zhang  <iyzhang@cs.washington.edu>
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

#ifndef _INDICUS_CLIENT_H_
#define _INDICUS_CLIENT_H_

#include "lib/assert.h"
#include "lib/keymanager.h"
#include "lib/latency.h"
#include "lib/message.h"
#include "lib/configuration.h"
#include "lib/udptransport.h"
#include "replication/ir/client.h"
#include "store/common/timestamp.h"
#include "store/common/truetime.h"
#include "store/common/frontend/client.h"
#include "store/common/frontend/bufferclient.h"
#include "store/indicusstore/shardclient.h"
#include "store/indicusstore/indicus-proto.pb.h"

#include <thread>
#include <set>

#define RESULT_COMMITTED 0
#define RESULT_USER_ABORTED 1
#define RESULT_SYSTEM_ABORTED 2
#define RESULT_MAX_RETRIES 3

namespace indicusstore {

class Client : public ::Client {
 public:
  Client(transport::Configuration *config, uint64_t id, int nShards,
      int nGroups, const std::vector<int> &closestReplicas,
      Transport *transport, partitioner part, bool syncCommit,
      uint64_t readMessages, uint64_t readQuorumSize, uint64_t readDepSize,
      bool signedMessages, bool validateProofs, KeyManager *keyManager,
      TrueTime timeserver = TrueTime(0,0));
  virtual ~Client();

  // Begin a transaction.
  virtual void Begin(begin_callback bcb, begin_timeout_callback btcb,
      uint32_t timeout) override;

  // Get the value corresponding to key.
  virtual void Get(const std::string &key, get_callback gcb,
      get_timeout_callback gtcb, uint32_t timeout = GET_TIMEOUT) override;

  // Set the value for the given key.
  virtual void Put(const std::string &key, const std::string &value,
      put_callback pcb, put_timeout_callback ptcb,
      uint32_t timeout = PUT_TIMEOUT) override;

  // Commit all Get(s) and Put(s) since Begin().
  virtual void Commit(commit_callback ccb, commit_timeout_callback ctcb,
      uint32_t timeout) override;

  // Abort all Get(s) and Put(s) since Begin().
  virtual void Abort(abort_callback acb, abort_timeout_callback atcb,
      uint32_t timeout) override;

 private:
  struct PendingRequest {
    PendingRequest(uint64_t id) : id(id), outstandingPhase1s(0),
        outstandingPhase2s(0), commitTries(0), maxRepliedTs(0UL),
        decision(proto::COMMIT), fast(true),
        startedPhase2(false), startedWriteback(false),
        callbackInvoked(false), timeout(0UL) {
    }

    ~PendingRequest() {
    }

    commit_callback ccb;
    commit_timeout_callback ctcb;
    uint64_t id;
    int outstandingPhase1s;
    int outstandingPhase2s;
    int commitTries;
    uint64_t maxRepliedTs;
    proto::CommitDecision decision;
    bool fast;
    bool startedPhase2;
    bool startedWriteback;
    bool callbackInvoked;
    uint32_t timeout;
    std::map<int, std::vector<proto::Phase1Reply>> phase1RepliesGrouped;
    std::map<int, std::vector<proto::SignedMessage>> signedPhase1RepliesGrouped;
    std::vector<proto::Phase2Reply> phase2Replies;
    std::vector<proto::SignedMessage> signedPhase2Replies;
    std::string txnDigest;
  };

  void Phase1(PendingRequest *req);
  void Phase1Callback(uint64_t reqId, int group, proto::CommitDecision decision,
      bool fast, const std::vector<proto::Phase1Reply> &phase1Replies,
      const std::vector<proto::SignedMessage> &signedPhase1Replies);
  void Phase1TimeoutCallback(int group, uint64_t reqId, int status);
  void HandleAllPhase1Received(PendingRequest *req);

  void Phase2(PendingRequest *req);
  void Phase2Callback(uint64_t reqId,
      const std::vector<proto::Phase2Reply> &phase2Replies,
      const std::vector<proto::SignedMessage> &signedPhase2Replies);
  void Phase2TimeoutCallback(int group, uint64_t reqId, int status);

  // Fallback logic
  
  //void Receive Full Dep or Receive Full Conflict. (this should be received as answer from a replica instead of P1R if tx is stalled on a dependency. Alternatively, this is the conflicting TX)
  //void Phase1_Rec   P1 do not need to be signed. Send p1 rec request to every replica in every involved shard.

  //void Phase1_Rec_Callback: Wait for either: Fast Path or Slow Path of p1r quorums. OR: if received p2r replies: Use those if f+1 received, or start election if inconsistent received.  P1_recR := (p1R, optional: p2R)_R.
  //We need to extend p2R messages to include views  (can interpret no view = v0 if that makes it easiest to not change current code).      Replicas must keep track of curr_view per TX as well.


  // 2 cases:
     //void Phase2_Rec    P2 need to be signed in order to enforce time-out, replicas will buffer until time-out.
  // 1: normal P2 (includes as proof P1R Quorum from all shards, or f+1 P2R from loggin shard)
    //void InvokeFallback
  // 2: Invoke election: Include signed Quorum of replica current views. Alternatively, if it makes things easier: Just send request to all replicas and have replicas use all to all. In this case we do not need signature proofs AND we can use Macs between replicas.


  //void Phase2_Rec_Callback: Receive a P2 from a view > 0. Try to assemble 4f+1 matching. (Keep a mapping from views to Sets in order to potentially do this for multiple views; GC old ones.)


  //void ReceiveNewView.  (Only necessary if doing the client driven view change. This happens if replicas tried to elect a FB, but timed out on a response because the FB is byz or crashed or whatever) Receive 3f+1 matching from higher view than last and start new Invocation
  //void FBWriteback: Should just be the normal writeback

  void Writeback(PendingRequest *req);

  bool IsParticipant(int g) const;

  /* Configuration State */
  transport::Configuration *config;
  // Unique ID for this client.
  uint64_t client_id;
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
  uint64_t readMessages;
  uint64_t readQuorumSize;
  uint64_t readDepSize;
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
  std::unordered_map<uint64_t, PendingRequest *> pendingReqs;

  /* Debug State */
  std::unordered_map<std::string, uint32_t> statInts;
  struct Latency_t executeLatency;
  struct Latency_t getLatency;
  size_t getIdx;
  struct Latency_t commitLatency;
};

} // namespace indicusstore

#endif /* _INDICUS_CLIENT_H_ */
