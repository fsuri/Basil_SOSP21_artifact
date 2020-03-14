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
  struct PendingRequest {
    PendingRequest(uint64_t id) : id(id), outstandingPhase1s(0),
        outstandingPhase2s(0), commitTries(0), maxRepliedTs(0UL),
        decision(proto::COMMIT), fast(true), prepareTimestamp(nullptr) {
    }

    ~PendingRequest() {
      if (prepareTimestamp != nullptr) {
        delete prepareTimestamp;
      }
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
    Timestamp *prepareTimestamp;
    bool callbackInvoked;
    std::vector<proto::Transaction> deps;
    std::map<int, std::vector<proto::Phase1Reply>> phase1RepliesGrouped;
    std::map<int, std::vector<proto::SignedMessage>> signedPhase1RepliesGrouped;
  };

  // Prepare function
  void Phase1(PendingRequest *req, uint32_t timeout);
  void Phase1Callback(uint64_t reqId, int group, proto::CommitDecision decision,
      bool fast, const std::vector<proto::Phase1Reply> &phase1Replies,
      const std::vector<proto::SignedMessage> &signedPhase1Replies);
  void Phase1TimeoutCallback(uint64_t reqId, int status, Timestamp ts);
  void HandleAllPhase1Received(PendingRequest *req);

  void Phase2(PendingRequest *req, uint32_t timeout);
  void Phase2Callback(uint64_t reqId, proto::CommitDecision decision);
  void Phase2TimeoutCallback(uint64_t reqId, int status);

  transport::Configuration *config;

  // Unique ID for this client.
  uint64_t client_id;

  // Ongoing transaction ID.
  uint64_t t_id;

  // Number of shards.
  uint64_t nshards;
  uint64_t ngroups;

  // Number of retries for current transaction.
  long retries;

  // List of participants in the ongoing transaction.
  std::set<int> participants;

  // Transport used by IR client proxies.
  Transport *transport;
  
  // Client for each shard
  std::vector<ShardClient *> bclient;

  partitioner part;
  
  bool syncCommit;
  bool signedMessages;

  uint64_t lastReqId;
  KeyManager *keyManager;
  // TrueTime server.
  TrueTime timeServer;
  
  std::unordered_map<uint64_t, PendingRequest *> pendingReqs;
  std::unordered_map<std::string, uint32_t> statInts;
};

} // namespace indicusstore

#endif /* _INDICUS_CLIENT_H_ */
