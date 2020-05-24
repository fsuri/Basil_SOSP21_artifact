// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * store/indicusstore/server.h:
 *   A single transactional server replica.
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

#ifndef _INDICUS_SERVER_H_
#define _INDICUS_SERVER_H_

#define CLIENTTIMEOUT  10    //currently miliseconds; adjust to whatever is a sensible time: dont know what is expected latency for 1 op

#include "lib/latency.h"
#include "lib/transport.h"
#include "store/common/backend/pingserver.h"
#include "store/server.h"
#include "store/common/partitioner.h"
#include "store/common/timestamp.h"
#include "store/common/truetime.h"
#include "store/indicusstore/common.h"
#include "store/indicusstore/store.h"
#include "store/indicusstore/indicus-proto.pb.h"
#include "store/indicusstore/batchsigner.h"
#include "store/indicusstore/verifier.h"
#include <sys/time.h>

#include <set>
#include <unordered_map>
#include <unordered_set>
#include <ctime>

namespace indicusstore {

class ServerTest;

enum OCCType {
  MVTSO = 0,
  TAPIR = 1
};

class Server : public TransportReceiver, public ::Server, public PingServer {
 public:
  Server(const transport::Configuration &config, int groupIdx, int idx,
      int numShards, int numGroups,
      Transport *transport, KeyManager *keyManager, Parameters params, uint64_t timeDelta,
      OCCType occType, Partitioner *part, unsigned int batchTimeoutMS,
      TrueTime timeServer = TrueTime(0, 0));
  virtual ~Server();

  virtual void ReceiveMessage(const TransportAddress &remote,
      const std::string &type, const std::string &data,
      void *meta_data) override;

  virtual void Load(const std::string &key, const std::string &value,
      const Timestamp timestamp) override;

  virtual inline Stats &GetStats() override { return stats; }

 private:
  friend class ServerTest;
  struct Value {
    std::string val;
    const proto::CommittedProof *proof;
  };

  void HandleRead(const TransportAddress &remote, const proto::Read &msg);
  void HandlePhase1(const TransportAddress &remote,
      proto::Phase1 &msg);
  void HandlePhase2(const TransportAddress &remote,
      const proto::Phase2 &msg);
  void HandleWriteback(const TransportAddress &remote,
      proto::Writeback &msg);
  void HandleAbort(const TransportAddress &remote, const proto::Abort &msg);

//Fallback protocol components
// Edit MVTSO-check: When we suspend a transaction waiting for a dependency, then after some timeout, we should send the full TX to the client (IF we have it - 1 correct replica is guaranteed to have it.)
// void HandleP1_Rec -> exec p1 if unreceived, reply with p1r, or p2r + dec_view  (Need to modify normal P2R message to contain view=0), current view
void HandlePhase1FB(const TransportAddress &remote, proto::Phase1FB &msg);
// void HandleP2_Rec -> Reply with p2 decision
// void HandleFB_Invoke -> send Elect message to FB based on views received. OR: Send all to all to other replicas (can use MACs to all replicas BESIDES the To-be-fallback) for next replica to elect.
// void HandleFB_Dec -> receive FB decision, verify whether majority was indeed confirmed and sends signed P2R to all interested clients (this must include the view from the decision)

//Fallback responsibilities
//void HandleFB_Elect: If 4f+1 Elect messages received -> form Decision based on majority, and forward the elect set (send FB_Dec) to all replicas in logging shard. (This includes the FB replica itself - Just skip ahead to HandleFB_Dec automatically: send P2R to clients)


void HandlePhase2FB(const TransportAddress &remote,proto::Phase2FB &msg);

void HandleInvokeFB(const TransportAddress &remote,proto::InvokeFB &msg); //DONT send back to remote, but instead to FB, calculate based on view. (need to include this in TX state thats kept locally.)

void HandleElectFB(const TransportAddress &remote,proto::ElectFB &msg);

void HandleDecisionFB(const TransportAddress &remote,proto::DecisionFB &msg); //DONT send back to remote, but instead to interested clients. (need to include list of interested clients as part of local tx state)

void HandleMoveView(const TransportAddress &remote,proto::MoveView &msg);


  proto::ConcurrencyControl::Result DoOCCCheck(
      uint64_t reqId, const TransportAddress &remote,
      const std::string &txnDigest, const proto::Transaction &txn,
      Timestamp &retryTs, proto::CommittedProof &conflict);
  proto::ConcurrencyControl::Result DoTAPIROCCCheck(
      const std::string &txnDigest, const proto::Transaction &txn,
      Timestamp &retryTs);
  proto::ConcurrencyControl::Result DoMVTSOOCCCheck(
      uint64_t reqId, const TransportAddress &remote,
      const std::string &txnDigest, const proto::Transaction &txn,
      proto::CommittedProof &conflict);

  void GetWriteTimestamps(
      std::unordered_map<std::string, std::set<Timestamp>> &writes);
  void GetWrites(
      std::unordered_map<std::string, std::vector<const proto::Transaction *>> &writes);
  void GetPreparedReadTimestamps(
      std::unordered_map<std::string, std::set<Timestamp>> &reads);
  void GetPreparedReads(
      std::unordered_map<std::string, std::vector<const proto::Transaction *>> &reads);
  void Prepare(const std::string &txnDigest, const proto::Transaction &txn);
  void GetCommittedWrites(const std::string &key, const Timestamp &ts,
      std::vector<std::pair<Timestamp, Value>> &writes);
  void Commit(const std::string &txnDigest, proto::Transaction *txn,
      proto::GroupedSignatures *groupedSigs, bool p1Sigs);
  void Abort(const std::string &txnDigest);
  void CheckDependents(const std::string &txnDigest);
  proto::ConcurrencyControl::Result CheckDependencies(
      const std::string &txnDigest);
  proto::ConcurrencyControl::Result CheckDependencies(
      const proto::Transaction &txn);
  bool CheckHighWatermark(const Timestamp &ts);
  void SendPhase1Reply(uint64_t reqId,
    proto::ConcurrencyControl::Result result,
    const proto::CommittedProof &conflict, const std::string &txnDigest,
    const TransportAddress &remote);
  void Clean(const std::string &txnDigest);
  void CleanDependencies(const std::string &txnDigest);
  void LookupP1Decision(const std::string &txnDigest, int64_t &myProcessId,
      proto::ConcurrencyControl::Result &myResult) const;
  void LookupP2Decision(const std::string &txnDigest,
      int64_t &myProcessId, proto::CommitDecision &myDecision) const;
  uint64_t DependencyDepth(const proto::Transaction *txn) const;
  void MessageToSign(::google::protobuf::Message* msg,
      proto::SignedMessage *signedMessage, signedCallback cb);
  proto::ReadReply *GetUnusedReadReply();
  proto::Phase1Reply *GetUnusedPhase1Reply();
  proto::Phase2Reply *GetUnusedPhase2Reply();
  void FreeReadReply(proto::ReadReply *reply);
  void FreePhase1Reply(proto::Phase1Reply *reply);
  void FreePhase2Reply(proto::Phase2Reply *reply);


  inline bool IsKeyOwned(const std::string &key) const {
    return static_cast<int>((*part)(key, numShards, groupIdx, dummyTxnGroups) % numGroups) == groupIdx;
  }

  const transport::Configuration &config;
  const int groupIdx;
  const int idx;
  const int numShards;
  const int numGroups;
  const int id;
  Transport *transport;
  const OCCType occType;
  Partitioner *part;
  const Parameters params;
  KeyManager *keyManager;
  const uint64_t timeDelta;
  TrueTime timeServer;
  BatchSigner *batchSigner;
  Verifier *verifier;

  /* Declare protobuf objects as members to avoid stack alloc/dealloc costs */
  proto::SignedMessage signedMessage;
  proto::Read read;
  proto::Phase1 phase1;
  proto::Phase2 phase2;
  proto::Writeback writeback;
  proto::Abort abort;

  proto::Write preparedWrite;
  proto::CommittedProof committedProof;
  proto::ConcurrencyControl concurrencyControl;
  proto::AbortInternal abortInternal;
  std::vector<int> dummyTxnGroups;

  std::vector<proto::ReadReply *> readReplies;
  std::vector<proto::Phase1Reply *> p1Replies;
  std::vector<proto::Phase2Reply *> p2Replies;
  proto::Phase1Reply phase1Reply;
  proto::Phase2Reply phase2Reply;
  proto::RelayP1 relayP1;
  proto::Phase1FB phase1FB;
  proto::Phase1FBReply phase1FBReply;
  proto::Phase2FB phase2FB;
  proto::Phase2FBReply phase2FBReply;
  proto::InvokeFB invokeFB;
  proto::ElectFB electFB;
  proto::DecisionFB decisionFB;
  proto::MoveView moveView;

  PingMessage ping;

//FALLBACK helper functions
//TODO: make strings call by ref.
  void SetP1(uint64_t reqId, std::string txnDigest, proto::ConcurrencyControl::Result &result, proto::CommittedProof &conflict);
  void SetP2(uint64_t reqId, std::string txnDigest, proto::CommitDecision &decision);
  void SendPhase1FBReply(uint64_t reqId, proto::Phase1Reply &p1r, proto::Phase2Reply &p2r, proto::Writeback &wb, const TransportAddress &remote,  std::string txnDigest, uint32_t response_case );

  void VerifyP2FB(const TransportAddress &remote, std::string &txnDigest, proto::Phase2FB &p2fb);
  bool VerifyViews(proto::InvokeFB &msg, uint32_t lG);
  void RelayP1(const TransportAddress &remote, proto::Transaction &tx, uint64_t conflict_id);

  VersionedKVStore<Timestamp, Value> store;
  // Key -> V
  std::unordered_map<std::string, std::set<std::tuple<Timestamp, Timestamp, const proto::CommittedProof *>>> committedReads;
  std::unordered_map<std::string, std::set<Timestamp>> rts;

  // Digest -> V
  std::unordered_map<std::string, proto::Transaction *> ongoing;
  std::unordered_map<std::string, std::pair<Timestamp, const proto::Transaction *>> prepared;
  std::unordered_map<std::string, std::set<const proto::Transaction *>> preparedReads;
  std::unordered_map<std::string, std::map<Timestamp, const proto::Transaction *>> preparedWrites;



  std::unordered_map<std::string, proto::ConcurrencyControl::Result> p1Decisions;
  std::unordered_map<std::string, proto::CommittedProof> p1Conflicts;
  std::unordered_map<std::string, proto::CommitDecision> p2Decisions;
  std::unordered_map<std::string, proto::CommittedProof *> committed;
  std::unordered_set<std::string> aborted;    //ADD Aborted proof to it.(in order to reply to Fallback)
  //creating new map to store writeback messages..  Need to find a better way, but suffices as placeholder

  //keep list of all remote addresses == interested client_seq_num
  std::unordered_map<std::string, std::unordered_set<const TransportAddress*>> interestedClients;
  //keep list of timeouts
  //std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> FBclient_timeouts;
  std::unordered_map<std::string, uint64_t> client_starttime;

  //keep list for exponential timeouts for views.
  std::unordered_map<std::string, uint64_t> FBtimeouts_start; //Timer start time
  std::unordered_map<std::string, uint64_t> exp_timeouts; //current exp timeout size.

  //keep list for current view.
  std::unordered_map<std::string, uint64_t> current_views;
  //keep list of the views in which the p2Decision is from: //TODO: add this to p2Decisions directly - doing this here so I do not touch any existing code.
  std::unordered_map<std::string, uint64_t> decision_views;

  std::unordered_map<std::string, std::unordered_set<const proto::SignedMessage*>> ElectQuorum;  //tuple contains view entry, set for that view and count of Commit vs Abort.
  std::unordered_map<std::string, std::pair<uint64_t, uint64_t>> ElectQuorum_meta;

  std::unordered_map<std::string, proto::Writeback> writebackMessages;

  std::unordered_map<std::string, std::unordered_set<std::string>> dependents; // Each V depends on K
  struct WaitingDependency {
    uint64_t reqId;
    const TransportAddress *remote;
    std::unordered_set<std::string> deps;
  };
  std::unordered_map<std::string, WaitingDependency> waitingDependencies; // K depends on each V

  Stats stats;
  std::unordered_set<std::string> active;
  Latency_t committedReadInsertLat;
  Latency_t verifyLat;
  Latency_t signLat;
};

} // namespace indicusstore

#endif /* _INDICUS_SERVER_H_ */
