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
#define CLIENTTIMEOUT  10 //100

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
#include <sys/time.h>
#include "store/common/stats.h"

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
      int nGroups, const std::vector<int> &closestReplicas, bool pingReplicas,
      Transport *transport, Partitioner *part, bool syncCommit,
      uint64_t readMessages, uint64_t readQuorumSize,
      Parameters params, KeyManager *keyManager, uint64_t phase1DecisionTimeout,
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

  inline const Stats &GetStats() const { return stats; }
 private:
   Stats stats;
   int fast_path_counter;
   int total_counter;

  struct PendingRequest {
    PendingRequest(uint64_t id) : id(id), outstandingPhase1s(0),
        outstandingPhase2s(0), commitTries(0), maxRepliedTs(0UL),
        decision(proto::COMMIT), fast(true), conflict_flag(false),
        startedPhase2(false), startedWriteback(false),
        callbackInvoked(false), timeout(0UL), slowAbortGroup(-1),
        startFB(false) {
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
    bool conflict_flag;
    bool startedPhase2;
    bool startedWriteback;
    bool callbackInvoked;
    uint32_t timeout;
    proto::GroupedSignatures p1ReplySigsGrouped;
    proto::GroupedSignatures p2ReplySigsGrouped;
    std::string txnDigest;
    int slowAbortGroup;
    int fastAbortGroup;
    proto::CommittedProof conflict;
    //added this for fallback handling
    proto::Transaction txn;
    proto::P2Replies p2Replies;
    bool startFB;
    std::vector<std::pair<proto::Phase1*, std::string>> RelayP1s;

  };

  void Phase1(PendingRequest *req);
  void Phase1Callback(uint64_t reqId, int group, proto::CommitDecision decision,
      bool fast, bool conflict_flag, const proto::CommittedProof &conflict,
      const std::map<proto::ConcurrencyControl::Result,
      proto::Signatures> &sigs);
  void Phase1CallbackProcessing(PendingRequest *req, int group,
      proto::CommitDecision decision, bool fast, bool conflict_flag,
      const proto::CommittedProof &conflict,
      const std::map<proto::ConcurrencyControl::Result, proto::Signatures> &sigs);
  void Phase1TimeoutCallback(int group, uint64_t reqId, int status);
  void HandleAllPhase1Received(PendingRequest *req);

  void Phase2(PendingRequest *req);
  void Phase2Processing(PendingRequest *req);
  void Phase2Callback(uint64_t reqId, int group,
      const proto::Signatures &p2ReplySigs);
  void Phase2TimeoutCallback(int group, uint64_t reqId, int status);
  void WritebackProcessing(PendingRequest *req);
  void Writeback(PendingRequest *req);

  // Fallback logic
  bool isDep(const std::string &txnDigest, proto::Transaction &Req_txn);
  bool StillActive(uint64_t conflict_id, std::string &txnDigest);
  void CleanFB(PendingRequest *pendingFB, std::string &txnDigest);
  void RelayP1callback(uint64_t reqId, proto::RelayP1 &relayP1);
  void RelayP1TimeoutCallback(uint64_t reqId);
  void Phase1FB(proto::Phase1 &p1, uint64_t conflict_id, const std::string &txnDigest);
  void Phase2FB(PendingRequest *req);
  void WritebackFB(PendingRequest *req);
  void Phase1FBcallbackA(uint64_t conflict_id, std::string txnDigest, int64_t group, proto::CommitDecision decision,
     bool fast, bool conflict_flag, const proto::CommittedProof &conflict, const std::map<proto::ConcurrencyControl::Result, proto::Signatures> &sigs);
  void FBHandleAllPhase1Received(PendingRequest *req);
  void Phase1FBcallbackB(uint64_t conflict_id, std::string txnDigest, int64_t group, proto::CommitDecision decision,
    const proto::P2Replies &p2replies);
  void Phase2FBcallback(uint64_t conflict_id, std::string txnDigest, int64_t group, proto::CommitDecision decision,
    const proto::Signatures &p2ReplySig);
  void WritebackFBcallback(uint64_t conflict_id, std::string txnDigest, proto::Transaction &fbtxn, proto::Writeback &wb);
  void InvokeFBcallback(uint64_t conflict_id, std::string txnDigest, int64_t group);
  //keep track of pending Fallback instances. Maps from txnDigest, req Id is oblivious to us.
  std::unordered_map<std::string, PendingRequest*> FB_instances;
  //TODO:: create another map from  <reqIds, string> and treat FB instances as normal reqID too.
       //TODO: should the FB_instances be part of a pendingRequest?
          // I.e. every pendingRequest has its nested pendingRequests? (That makes it too hard to find). Need flat hierarchy.
  //Question: How can client have multiple pendingReqs?
  //TODO: would this simplify having a deeper depth?
  // --> would allow normal OCC handling on Wait results at the server?

  //flag for OCC check: put into separate original client and interested client list: SendWait Reply to both.
  //Change all structures to -> original client + interested client. This makes it so that an original client
  //will not receive a fallback message. XXX however, do need to make it so that an original client can start
  //InvokeFB and consequently also receive P2FB messages...

  //1) sigs, 2) parallelism is sketch, 3) Wait handling

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
  Partitioner *part;
  bool syncCommit;
  const bool pingReplicas;
  const uint64_t readMessages;
  const uint64_t readQuorumSize;
  const Parameters params;
  KeyManager *keyManager;
  Verifier *verifier;
  Stats dummyStats;
  // TrueTime server.
  TrueTime timeServer;
  bool failureActive;

  bool first;
  bool startedPings;

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

  std::unordered_map<uint64_t, uint64_t> pendingReqs_starttime;

  inline static bool sortReadByKey(const ReadMessage &lhs, const ReadMessage &rhs) { return lhs.key() < rhs.key(); }
  inline static bool sortWriteByKey(const WriteMessage &lhs, const WriteMessage &rhs) { return lhs.key() < rhs.key(); }


  /* Debug State */
  std::unordered_map<std::string, uint32_t> statInts;
  struct Latency_t executeLatency;
  struct Latency_t getLatency;
  size_t getIdx;
  struct Latency_t commitLatency;
};

} // namespace indicusstore

#endif /* _INDICUS_CLIENT_H_ */
