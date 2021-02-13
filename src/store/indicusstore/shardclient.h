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
#include "store/indicusstore/phase1validator.h"
#include "store/common/pinginitiator.h"

#include <map>
#include <string>
#include <vector>

namespace indicusstore {

typedef std::function<void(int, const std::string &,
    const std::string &, const Timestamp &, const proto::Dependency &,
    bool, bool)> read_callback;
typedef std::function<void(int, const std::string &)> read_timeout_callback;

typedef std::function<void(proto::CommitDecision, bool, bool,
    const proto::CommittedProof &,
    const std::map<proto::ConcurrencyControl::Result, proto::Signatures> &)> phase1_callback;
typedef std::function<void(int)> phase1_timeout_callback;

typedef std::function<void(const proto::Signatures &)> phase2_callback;
typedef std::function<void(int)> phase2_timeout_callback;

typedef std::function<void()> writeback_callback;
typedef std::function<void(int)> writeback_timeout_callback;


//Fallback typedefs:
typedef std::function<void(proto::RelayP1 &)> relayP1_callback;
typedef std::function<void(const std::string &, proto::RelayP1 &)> relayP1FB_callback;

typedef std::function<void(proto::CommitDecision, bool, bool, const proto::CommittedProof &,
  const std::map<proto::ConcurrencyControl::Result, proto::Signatures> &)> phase1FB_callbackA;

typedef std::function<void(proto::CommitDecision, const proto::P2Replies &)> phase1FB_callbackB;

typedef std::function<void(proto::CommitDecision, const proto::Signatures &)> phase2FB_callback;

typedef std::function<void(proto::Writeback &)> writebackFB_callback;

typedef std::function<void()> invokeFB_callback;

typedef std::function<void()> viewQuorum_callback;

class ShardClient : public TransportReceiver, public PingInitiator, public PingTransport {
 public:
  ShardClient(transport::Configuration *config, Transport *transport,
      uint64_t client_id, int group, const std::vector<int> &closestReplicas,
      bool pingReplicas,
      Parameters params, KeyManager *keyManager, Verifier *verifier,
      TrueTime &timeServer, uint64_t phase1DecisionTimeout);
  virtual ~ShardClient();

  virtual void ReceiveMessage(const TransportAddress &remote,
      const std::string &type, const std::string &data,
      void *meta_data) override;

  // Begin a transaction.
  virtual void Begin(uint64_t id);

  // Get the value corresponding to key.
  virtual void Get(uint64_t id, const std::string &key, const TimestampMessage &ts,
      uint64_t readMessages, uint64_t rqs, uint64_t rds, read_callback gcb,
      read_timeout_callback gtcb, uint32_t timeout);

  // Set the value for the given key.
  virtual void Put(uint64_t id, const std::string &key,
      const std::string &value, put_callback pcb, put_timeout_callback ptcb,
      uint32_t timeout);

  virtual void Phase1(uint64_t id, const proto::Transaction &transaction, const std::string &txnDigest,
    phase1_callback pcb, phase1_timeout_callback ptcb, relayP1_callback rcb, uint32_t timeout);
  virtual void Phase2(uint64_t id, const proto::Transaction &transaction,
      const std::string &txnDigest, proto::CommitDecision decision,
      const proto::GroupedSignatures &groupedSigs, phase2_callback pcb,
      phase2_timeout_callback ptcb, uint32_t timeout);
  virtual void Writeback(uint64_t id, const proto::Transaction &transaction, const std::string &txnDigest,
    proto::CommitDecision decision, bool fast, bool conflict_flag, const proto::CommittedProof &conflict,
    const proto::GroupedSignatures &p1Sigs, const proto::GroupedSignatures &p2Sigs);
  //overloaded function for fallback
  virtual void WritebackFB(const proto::Transaction &transaction, const std::string &txnDigest,
      proto::CommitDecision decision, bool fast, const proto::CommittedProof &conflict,
      const proto::GroupedSignatures &p1Sigs, const proto::GroupedSignatures &p2Sigs);

  virtual void Abort(uint64_t id, const TimestampMessage &ts);
  virtual bool SendPing(size_t replica, const PingMessage &ping);

//public fallback functions:
  virtual void CleanFB(std::string &txnDigest);
  virtual void Phase1FB(uint64_t reqId, proto::Transaction &txn, const std::string &txnDigest, phase1FB_callbackA p1FBcbA,
    phase1FB_callbackB p1FBcbB, phase2FB_callback p2FBcb, writebackFB_callback wbFBcb, invokeFB_callback invFBcb);
  virtual void Phase2FB(uint64_t id,const proto::Transaction &txn, const std::string &txnDigest,proto::CommitDecision decision,
    const proto::GroupedSignatures &groupedSigs);
  //overloaded for different p2 alternative
  virtual void Phase2FB(uint64_t id,const proto::Transaction &txn, const std::string &txnDigest,proto::CommitDecision decision,
    const proto::P2Replies &p2Replies);
  virtual void WritebackFB_fast(std::string txnDigest, proto::Writeback &wb); //fix bracket
  virtual void InvokeFB(uint64_t conflict_id, std::string txnDigest, proto::Transaction &txn, proto::CommitDecision decision,
    proto::P2Replies &p2Replies);


 private:
  struct PendingQuorumGet {
    PendingQuorumGet(uint64_t reqId) : reqId(reqId),
        numReplies(0UL), numOKReplies(0UL), hasDep(false),
        firstCommittedReply(true) { }
    ~PendingQuorumGet() { }
    uint64_t reqId;
    std::string key;
    Timestamp rts;
    uint64_t rqs;
    uint64_t rds;
    Timestamp maxTs;
    std::string maxValue;
    uint64_t numReplies;
    uint64_t numOKReplies;
    std::map<Timestamp, std::pair<proto::Write, uint64_t>> prepared;
    std::map<Timestamp, proto::Signatures> preparedSigs;
    proto::Dependency dep;
    bool hasDep;
    read_callback gcb;
    read_timeout_callback gtcb;
    bool firstCommittedReply;
  };

  struct PendingPhase1 {
    PendingPhase1(uint64_t reqId, int group, const proto::Transaction &txn,
        const std::string &txnDigest, const transport::Configuration *config,
        KeyManager *keyManager, Parameters params, Verifier *verifier) :
        reqId(reqId), requestTimeout(nullptr), decisionTimeout(nullptr),
        decisionTimeoutStarted(false), txn_(txn), txnDigest_(txnDigest),
        p1Validator(group, &txn_, &txnDigest_, config, keyManager, params,
            verifier),
        decision(proto::ABORT), fast(false), conflict_flag(false) { }
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
    std::map<proto::ConcurrencyControl::Result, proto::Signatures> p1ReplySigs;
    phase1_callback pcb;
    phase1_timeout_callback ptcb;
    proto::Transaction txn_;
    std::string txnDigest_;
    Phase1Validator p1Validator;
    proto::CommitDecision decision;
    bool fast;
    bool conflict_flag;
    proto::CommittedProof conflict;
    //relay Callbacks
    uint64_t client_seq_num;
    relayP1_callback rcb;
  };


  struct PendingPhase2 {
    PendingPhase2() : requestTimeout(nullptr), matchingReplies(0UL) {}
    PendingPhase2(uint64_t reqId, proto::CommitDecision decision) : reqId(reqId),
        decision(decision), requestTimeout(nullptr), matchingReplies(0UL) { }
    ~PendingPhase2() {
      if (requestTimeout != nullptr) {
        delete requestTimeout;
      }
    }
    uint64_t reqId;
    proto::CommitDecision decision;
    //FALLBACK MEANS VIEW IS necessary
    uint64_t decision_view;  //can omit this for all requests that came from view = 0 because signature matches.
    //TODO: Need to add decision view checks eveywhere.
    Timeout *requestTimeout;
    std::unordered_set<uint64_t> process_ids; //TODO: add check to avoid duplicates.
    proto::Signatures p2ReplySigs;
    uint64_t matchingReplies;
    phase2_callback pcb;
    phase2_timeout_callback ptcb;
  };

  struct SignedView {
    SignedView() {}
    SignedView(uint64_t v): view(v) {}
    SignedView(uint64_t v, proto::SignedMessage s_v): view(v), signed_view(s_v) {}
    ~SignedView(){}

    uint64_t view;
    proto::SignedMessage signed_view;
  };

  //Fallback request
  struct PendingFB {
    PendingFB() : max_decision_view(0UL), p1(true), last_view(0), max_view(0) {}
    ~PendingFB(){
      delete pendingP1;
    }

    //TODO: pendingP1, pendingP2, Signed View need not be a pointer?
    PendingPhase1 *pendingP1;
    uint64_t max_decision_view;
    std::map<uint64_t, std::map<proto::CommitDecision, PendingPhase2>> pendingP2s;
    proto::Signatures p2ReplySigs;
    std::map<proto::CommitDecision, proto::P2Replies> p2Replies; //These must be from the same group, but can differ in view.
    std::unordered_set<uint64_t> process_ids;
    bool p1; //DISTINGUISHES IN WHICH PHASE WE ARE WHEN HANDLING P2s

    std::map<uint64_t, SignedView> current_views;
    std::map<uint64_t, std::set<uint64_t>> view_levels; //maps from view to ids  in that view
    uint64_t last_view;
    uint64_t max_view;  //we will propose max_view, but only if its bigger than last_view; otherwise we need better votes.
    bool catchup;
    //std::set<uint64_t> existing_levels;

    //TODO: add different callbacks
    relayP1FB_callback rcb;
    writebackFB_callback wbFBcb;
    phase1FB_callbackA p1FBcbA; // can use a lot from phase1_callback (edited to include the f+1 p2 case + sends a P2FB message instead)
    phase1FB_callbackB p1FBcbB;
    phase2FB_callback p2FBcb; // callback in case that we finish normal p2, can return just as if it was the normal protocol?
    invokeFB_callback invFBcb;

    // manage Invocation start
    viewQuorum_callback view_invoker;
    bool call_invokeFB;

  };

  std::unordered_map<std::string, PendingFB*> pendingFallbacks; //map from txnDigests to their fallback instances.



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

  void Phase1Decision(uint64_t reqId);
  void Phase1Decision(
      std::unordered_map<uint64_t, PendingPhase1 *>::iterator itr);

  //multithreaded options:
  void HandleReadReplyMulti(proto::ReadReply* reply);
  void HandleReadReplyCB1(proto::ReadReply*reply);
  void HandleReadReplyCB2(proto::ReadReply* reply, proto::Write *write);


//multithreading Support
//TODO seperate the locks for these!!!!
  std::mutex writeProtoMutex;
  std::mutex readProtoMutex;
  std::mutex p1ProtoMutex;
  std::mutex p2ProtoMutex;

  proto::Write *GetUnusedWrite();
  proto::ReadReply *GetUnusedReadReply();
  proto::Phase1Reply *GetUnusedPhase1Reply();
  proto::Phase2Reply *GetUnusedPhase2Reply();

  void FreeWrite(proto::Write *write);
  void FreeReadReply(proto::ReadReply *reply);
  void FreePhase1Reply(proto::Phase1Reply *reply);
  void FreePhase2Reply(proto::Phase2Reply *reply);

  std::vector<proto::Write *> writes;
  std::vector<proto::ReadReply *> readReplies;
  std::vector<proto::Phase1Reply *> p1Replies;
  std::vector<proto::Phase2Reply *> p2Replies;


  //private fallback functions
  void HandlePhase1Relay(proto::RelayP1 &relayP1);
  void HandlePhase1FBReply(proto::Phase1FBReply &p1fbr);
  void ProcessP1FBR(proto::Phase1Reply &reply, PendingFB *pendingFB, const std::string &txnDigest);
  void Phase1FBDecision(PendingFB *pendingFB);
  void ProcessP2FBR(proto::Phase2Reply &reply, PendingFB *pendingFB, const std::string &txnDigest);
  void HandlePhase2FBReply(proto::Phase2FBReply &p2fbr);
  void HandleSendViewMessage(proto::SendView &sendView);
  void ComputeMaxLevel(PendingFB *pendingFB);
  void UpdateViewStructure(PendingFB *pendingFB, const proto::AttachedView &ac);

  inline size_t GetNthClosestReplica(size_t idx) const {
    if (pingReplicas && GetOrderedReplicas().size() > 0) {
      return GetOrderedReplicas()[idx];
    } else {
      return closestReplicas[idx];
    }
  }

  const uint64_t client_id; // Unique ID for this client.
  Transport *transport; // Transport layer.
  transport::Configuration *config;
  const int group; // which shard this client accesses
  TrueTime &timeServer;
  const bool pingReplicas;
  const Parameters params;
  KeyManager *keyManager;
  Verifier *verifier;
  const uint64_t phase1DecisionTimeout;
  std::vector<int> closestReplicas;

  uint64_t lastReqId;
  proto::Transaction txn;
  std::map<std::string, std::string> readValues;

  std::unordered_map<uint64_t, PendingQuorumGet *> pendingGets;
  std::unordered_map<uint64_t, PendingPhase1 *> pendingPhase1s;
  std::unordered_map<uint64_t, PendingPhase2 *> pendingPhase2s;
  std::unordered_map<uint64_t, PendingAbort *> pendingAborts;

  //keep additional maps for this from txnDigest ->Pending For Fallback instances?

  proto::Read read;
  proto::Phase1 phase1;
  proto::Phase2 phase2;
  proto::Writeback writeback;
  proto::Abort abort;
  proto::ReadReply readReply;
  proto::Phase1Reply phase1Reply;
  proto::Phase2Reply phase2Reply;
  PingMessage ping;


  //FALLBACK
  proto::RelayP1 relayP1;
  proto::Phase1FB phase1FB;
  proto::Phase1FBReply phase1FBReply;
  proto::Phase2FB phase2FB;
  proto::Phase2FBReply phase2FBReply;
  proto::InvokeFB invokeFB;
  proto::SendView sendView;


  proto::Write validatedPrepared;
  proto::ConcurrencyControl validatedCC;
  proto::Phase2Decision validatedP2Decision;
};

} // namespace indicusstore

#endif /* _INDICUS_SHARDCLIENT_H_ */
