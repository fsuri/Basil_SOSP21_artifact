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

#include <set>
#include <unordered_map>
#include <unordered_set>

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
      Transport *transport, KeyManager *keyManager, bool signedMessages,
      bool validateProofs, bool hashDigest, uint64_t timeDelta, OCCType occType,
      Partitioner *part, uint64_t readDepSize,
      TrueTime timeServer = TrueTime(0, 0));
  virtual ~Server();

  virtual void ReceiveMessage(const TransportAddress &remote,
      const std::string &type, const std::string &data,
      void *meta_data) override;

  virtual void Load(const string &key, const string &value,
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
// void HandleP1_Rec -> exec p1 if unreceived, reply with p1r, or p2r + dec_view  (Need to modify normal P2R message to contain view=0), current view
// void HandleP2_Rec -> Reply with p2 decision
// void HandleFB_Invoke -> send Elect message to FB based on views
// void HandleFB_Dec -> receive FB decision and send to all interested

//Fallback responsibilities
//void HandleFB_Elect: If 4f+1 received -> send HandleFB_Dec to all replicas in logging shard

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

  void GetPreparedWriteTimestamps(
      std::unordered_map<std::string, std::set<Timestamp>> &writes);
  void GetPreparedWrites(
      std::unordered_map<std::string, std::vector<const proto::Transaction *>> &writes);
  void GetPreparedReadTimestamps(
      std::unordered_map<std::string, std::set<Timestamp>> &reads);
  void GetPreparedReads(
      std::unordered_map<std::string, std::vector<const proto::Transaction *>> &reads);
  void Prepare(const std::string &txnDigest, const proto::Transaction &txn);
  void GetCommittedWrites(const std::string &key, const Timestamp &ts,
      std::vector<std::pair<Timestamp, Value>> &writes);
  void Commit(const std::string &txnDigest, proto::Transaction *txn,
      const proto::GroupedSignatures &groupedSigs, bool p1Sigs);
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
  const uint64_t readDepSize;
  const bool signedMessages;
  const bool validateProofs;
  const bool hashDigest;
  KeyManager *keyManager;
  const uint64_t timeDelta;
  TrueTime timeServer;

  /* Declare protobuf objects as members to avoid stack alloc/dealloc costs */
  proto::SignedMessage signedMessage;
  proto::Read read;
  proto::Phase1 phase1;
  proto::Phase2 phase2;
  proto::Writeback writeback;
  proto::Abort abort;

  proto::ReadReply readReply;
  proto::Phase1Reply phase1Reply;
  proto::Phase2Reply phase2Reply;

  proto::Transaction mostRecent;
  proto::PreparedWrite preparedWrite;
  proto::CommittedProof committedProof;
  proto::ConcurrencyControl concurrencyControl;
  proto::AbortInternal abortInternal;
  std::vector<int> dummyTxnGroups;

  PingMessage ping;

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
  std::unordered_map<std::string, proto::CommitDecision> p2Decisions;
  std::unordered_map<std::string, proto::CommittedProof *> committed;
  std::unordered_set<std::string> aborted;
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
