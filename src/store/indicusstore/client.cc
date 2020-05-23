// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * store/indicusstore/client.cc:
 *   Client to INDICUS transactional storage system.
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

#include "store/indicusstore/client.h"

#include "store/indicusstore/common.h"
#include <sys/time.h>

namespace indicusstore {

using namespace std;

Client::Client(transport::Configuration *config, uint64_t id, int nShards,
    int nGroups,
    const std::vector<int> &closestReplicas, bool pingReplicas, Transport *transport,
    Partitioner *part, bool syncCommit, uint64_t readMessages,
    uint64_t readQuorumSize, Parameters params,
    KeyManager *keyManager, TrueTime timeServer)
    : config(config), client_id(id), nshards(nShards), ngroups(nGroups),
    transport(transport), part(part), syncCommit(syncCommit), pingReplicas(pingReplicas),
    readMessages(readMessages), readQuorumSize(readQuorumSize),
    params(params),
    keyManager(keyManager),
    timeServer(timeServer), first(true), startedPings(false),
    client_seq_num(0UL), lastReqId(0UL), getIdx(0UL) {

  Debug("Initializing Indicus client with id [%lu] %lu", client_id, nshards);

  /* Start a client for each shard. */
  for (uint64_t i = 0; i < ngroups; i++) {
    bclient.push_back(new ShardClient(config, transport, client_id, i,
        closestReplicas, pingReplicas, params,
        keyManager, timeServer));
  }

  Debug("Indicus client [%lu] created! %lu %lu", client_id, nshards,
      bclient.size());
  _Latency_Init(&executeLatency, "execute");
  _Latency_Init(&getLatency, "get");
  _Latency_Init(&commitLatency, "commit");
}

Client::~Client()
{
  Latency_Dump(&executeLatency);
  Latency_Dump(&getLatency);
  Latency_Dump(&commitLatency);
    for (auto b : bclient) {
        delete b;
    }
}

/* Begins a transaction. All subsequent operations before a commit() or
 * abort() are part of this transaction.
 */
void Client::Begin(begin_callback bcb, begin_timeout_callback btcb,
      uint32_t timeout) {
  transport->Timer(0, [this, bcb, btcb, timeout]() {
    if (pingReplicas) {
      if (!first && !startedPings) {
        startedPings = true;
        for (auto sclient : bclient) {
          sclient->StartPings();
        }
      }
      first = false;
    }

    Latency_Start(&executeLatency);
    client_seq_num++;
    Debug("BEGIN [%lu]", client_seq_num);

    txn = proto::Transaction();
    txn.set_client_id(client_id);
    txn.set_client_seq_num(client_seq_num);
    // Optimistically choose a read timestamp for all reads in this transaction
    txn.mutable_timestamp()->set_timestamp(timeServer.GetTime());
    txn.mutable_timestamp()->set_id(client_id);
    bcb(client_seq_num);
  });
}

void Client::Get(const std::string &key, get_callback gcb,
    get_timeout_callback gtcb, uint32_t timeout) {
  transport->Timer(0, [this, key, gcb, gtcb, timeout]() {

    // Latency_Start(&getLatency);

    Debug("GET[%lu:%lu] for key %s", client_id, client_seq_num,
        BytesToHex(key, 16).c_str());

    // Contact the appropriate shard to get the value.
    std::vector<int> txnGroups(txn.involved_groups().begin(), txn.involved_groups().end());
    int i = (*part)(key, nshards, -1, txnGroups) % ngroups;

    // If needed, add this shard to set of participants and send BEGIN.
    if (!IsParticipant(i)) {
      txn.add_involved_groups(i);
      bclient[i]->Begin(client_seq_num);
    }

    read_callback rcb = [gcb, this](int status, const std::string &key,
        const std::string &val, const Timestamp &ts, const proto::Dependency &dep,
        bool hasDep, bool addReadSet) {
      uint64_t ns = 0; //Latency_End(&getLatency);
      if (Message_DebugEnabled(__FILE__)) {
        Debug("GET[%lu:%lu] Callback for key %s with %lu bytes and ts %lu.%lu after %luus.",
            client_id, client_seq_num, BytesToHex(key, 16).c_str(), val.length(),
            ts.getTimestamp(), ts.getID(), ns / 1000);
        if (hasDep) {
          Debug("GET[%lu:%lu] Callback for key %s with dep ts %lu.%lu.",
              client_id, client_seq_num, BytesToHex(key, 16).c_str(),
              dep.write().prepared_timestamp().timestamp(),
              dep.write().prepared_timestamp().id());
        }
      }
      if (addReadSet) {
        ReadMessage *read = txn.add_read_set();
        read->set_key(key);
        ts.serialize(read->mutable_readtime());
      }
      if (hasDep) {
        *txn.add_deps() = dep;
      }
      gcb(status, key, val, ts);
    };
    read_timeout_callback rtcb = gtcb;

    // Send the GET operation to appropriate shard.
    bclient[i]->Get(client_seq_num, key, txn.timestamp(), readMessages,
        readQuorumSize, params.readDepSize, rcb, rtcb, timeout);
  });
}

void Client::Put(const std::string &key, const std::string &value,
    put_callback pcb, put_timeout_callback ptcb, uint32_t timeout) {
  transport->Timer(0, [this, key, value, pcb, ptcb, timeout]() {
    Debug("PUT[%lu:%lu] for key %s", client_id, client_seq_num, BytesToHex(key,
          16).c_str());

    // Contact the appropriate shard to set the value.
    std::vector<int> txnGroups(txn.involved_groups().begin(), txn.involved_groups().end());
    int i = (*part)(key, nshards, -1, txnGroups) % ngroups;

    // If needed, add this shard to set of participants and send BEGIN.
    if (!IsParticipant(i)) {
      txn.add_involved_groups(i);
      bclient[i]->Begin(client_seq_num);
    }

    WriteMessage *write = txn.add_write_set();
    write->set_key(key);
    write->set_value(value);

    // Buffering, so no need to wait.
    bclient[i]->Put(client_seq_num, key, value, pcb, ptcb, timeout);
  });
}

void Client::Commit(commit_callback ccb, commit_timeout_callback ctcb,
    uint32_t timeout) {
  transport->Timer(0, [this, ccb, ctcb, timeout]() {
    uint64_t ns = Latency_End(&executeLatency);
    Latency_Start(&commitLatency);

    PendingRequest *req = new PendingRequest(client_seq_num);
    pendingReqs[client_seq_num] = req;
    req->txn = txn; //Is this a copy or just reference?
    req->ccb = ccb;
    req->ctcb = ctcb;
    req->callbackInvoked = false;
    req->txnDigest = TransactionDigest(txn, params.hashDigest);
    req->timeout = timeout;
    stats.Increment("txn_groups_" + std::to_string(txn.involved_groups().size()));
    Phase1(req);
  });
}

void Client::Phase1(PendingRequest *req) {
  Debug("PHASE1 [%lu:%lu] at %lu", client_id, client_seq_num,
      txn.timestamp().timestamp());

  UW_ASSERT(txn.involved_groups().size() > 0);

//p1 timer for fallbacks
  struct timeval tv;
  gettimeofday(&tv, NULL);
  uint64_t current_time = (tv.tv_sec*1000000+tv.tv_usec)/1000;
  pendingReqs_starttime[req->id] = current_time;


  for (auto group : txn.involved_groups()) {
    bclient[group]->Phase1(client_seq_num, txn, req->txnDigest, std::bind(
          &Client::Phase1Callback, this, req->id, group, std::placeholders::_1,
          std::placeholders::_2, std::placeholders::_3, std::placeholders::_4),
        std::bind(&Client::Phase1TimeoutCallback, this, group, req->id,
          std::placeholders::_1), std::bind(&Client::RelayP1callback, this, std::placeholders::_1), req->timeout);
    req->outstandingPhase1s++;
  }

  //TODO: Add the RelayP1 callback here.  shard client should then make it part of PendingP1, and use it after mvtso.
}

void Client::Phase1Callback(uint64_t txnId, int group,
    proto::CommitDecision decision, bool fast,
    const proto::CommittedProof &conflict,
    const std::map<proto::ConcurrencyControl::Result, proto::Signatures> &sigs) {
  auto itr = this->pendingReqs.find(txnId);
  if (itr == this->pendingReqs.end()) {
    Debug("Phase1Callback for terminated request %lu (txn already committed"
        " or aborted.", txnId);
    return;
  }

  Debug("PHASE1[%lu:%lu] callback decision %d from group %d", client_id,
      client_seq_num, decision, group);

  PendingRequest *req = itr->second;
  if (req->startedPhase2 || req->startedWriteback) {
    Debug("Already started Phase2/Writeback for request id %lu. Ignoring Phase1"
        " response from group %d.", txnId, group);
    return;
  }

  if (decision == proto::ABORT && fast) {
    if (params.validateProofs) {
      req->conflict = conflict;
    }
  }

  if (params.validateProofs && params.signedMessages) {
    if (decision == proto::ABORT && !fast) {
      auto itr = sigs.find(proto::ConcurrencyControl::ABSTAIN);
      UW_ASSERT(itr != sigs.end());
      Debug("Have %d ABSTAIN replies from group %d.", itr->second.sigs_size(),
          group);
      (*req->p1ReplySigsGrouped.mutable_grouped_sigs())[group] = itr->second;
      req->slowAbortGroup = group;
    } else if (decision == proto::COMMIT) {
      auto itr = sigs.find(proto::ConcurrencyControl::COMMIT);
      UW_ASSERT(itr != sigs.end());
      Debug("Have %d COMMIT replies from group %d.", itr->second.sigs_size(),
          group);
      (*req->p1ReplySigsGrouped.mutable_grouped_sigs())[group] = itr->second;
    }
  }

  if (fast && decision == proto::ABORT) {
    req->fast = true;
  } else {
    req->fast = req->fast && fast;
  }

  --req->outstandingPhase1s;
  switch(decision) {
    case proto::COMMIT:
      break;
    case proto::ABORT:
      // abort!
      req->decision = proto::ABORT;
      req->outstandingPhase1s = 0;
      break;
    default:
      break;
  }

  if (req->outstandingPhase1s == 0) {
    HandleAllPhase1Received(req);
    pendingReqs_starttime.erase(txnId);
  }
}

void Client::Phase1TimeoutCallback(int group, uint64_t txnId, int status) {
  auto itr = this->pendingReqs.find(txnId);
  if (itr == this->pendingReqs.end()) {
    return;
  }

  PendingRequest *req = itr->second;
  if (req->startedPhase2 || req->startedWriteback) {
    return;
  }

  Warning("PHASE1[%lu:%lu] group %d timed out.", client_id, txnId, group);

  Phase1(req);
}

void Client::HandleAllPhase1Received(PendingRequest *req) {
  Debug("All PHASE1's [%lu] received", client_seq_num);
  if (req->fast) {
    Writeback(req);
  } else {
    // slow path, must log final result to 1 group
    Phase2(req);
  }
}

void Client::Phase2(PendingRequest *req) {
  uint8_t groupIdx = req->txnDigest[0];
  groupIdx = groupIdx % txn.involved_groups_size();
  UW_ASSERT(groupIdx < txn.involved_groups_size());
  int64_t logGroup = txn.involved_groups(groupIdx);

  Debug("PHASE2[%lu:%lu][%s] logging to group %ld", client_id, client_seq_num,
      BytesToHex(req->txnDigest, 16).c_str(), logGroup);

  if (params.validateProofs && params.signedMessages) {
    if (req->decision == proto::ABORT) {
      UW_ASSERT(req->slowAbortGroup >= 0);
      UW_ASSERT(req->p1ReplySigsGrouped.grouped_sigs().find(req->slowAbortGroup) != req->p1ReplySigsGrouped.grouped_sigs().end());
      while (req->p1ReplySigsGrouped.grouped_sigs().size() > 1) {
        auto itr = req->p1ReplySigsGrouped.mutable_grouped_sigs()->begin();
        if (itr->first == req->slowAbortGroup) {
          itr++;
        }
        req->p1ReplySigsGrouped.mutable_grouped_sigs()->erase(itr);
      }
    }

    uint64_t quorumSize = req->decision == proto::COMMIT ?
        SlowCommitQuorumSize(config) : SlowAbortQuorumSize(config);
    for (auto &groupSigs : *req->p1ReplySigsGrouped.mutable_grouped_sigs()) {
      while (static_cast<uint64_t>(groupSigs.second.sigs_size()) > quorumSize) {
        groupSigs.second.mutable_sigs()->RemoveLast();
      }
    }
  }

  req->startedPhase2 = true;
  bclient[logGroup]->Phase2(client_seq_num, txn, req->txnDigest, req->decision,
      req->p1ReplySigsGrouped,
      std::bind(&Client::Phase2Callback, this, req->id, logGroup,
        std::placeholders::_1),
      std::bind(&Client::Phase2TimeoutCallback, this, logGroup, req->id,
        std::placeholders::_1), req->timeout);
}

void Client::Phase2Callback(uint64_t txnId, int group,
    const proto::Signatures &p2ReplySigs) {
  auto itr = this->pendingReqs.find(txnId);
  if (itr == this->pendingReqs.end()) {
    Debug("Phase2Callback for terminated request id %lu (txn already committed or aborted.", txnId);
    return;
  }

  Debug("PHASE2[%lu:%lu] callback", client_id, txnId);

  PendingRequest *req = itr->second;

  if (req->startedWriteback) {
    Debug("Already started Writeback for request id %lu. Ignoring Phase2 response.",
        txnId);
    return;
  }

  if (params.validateProofs && params.signedMessages) {
    (*req->p2ReplySigsGrouped.mutable_grouped_sigs())[group] = p2ReplySigs;
  }

  Writeback(req);
}

void Client::Phase2TimeoutCallback(int group, uint64_t txnId, int status) {
  auto itr = this->pendingReqs.find(txnId);
  if (itr == this->pendingReqs.end()) {
    return;
  }

  PendingRequest *req = itr->second;
  if (req->startedWriteback) {
    return;
  }

  Warning("PHASE2[%lu:%lu] group %d timed out.", client_id, txnId, group);

  Phase2(req);
}

void Client::Writeback(PendingRequest *req) {
  Debug("WRITEBACK[%lu:%lu]", client_id, req->id);

  req->startedWriteback = true;

  transaction_status_t result;
  switch (req->decision) {
    case proto::COMMIT: {
      Debug("WRITEBACK[%lu:%lu][%s] COMMIT.", client_id, req->id,
          BytesToHex(req->txnDigest, 16).c_str());
      result = COMMITTED;
      break;
    }
    case proto::ABORT: {
      result = ABORTED_SYSTEM;
      Debug("WRITEBACK[%lu:%lu][%s] ABORT.", client_id, req->id,
          BytesToHex(req->txnDigest, 16).c_str());
      break;
    }
    default: {
      NOT_REACHABLE();
    }
  }

  for (auto group : txn.involved_groups()) {
    bclient[group]->Writeback(client_seq_num, txn, req->txnDigest,
        req->decision, req->fast, req->conflict, req->p1ReplySigsGrouped,
        req->p2ReplySigsGrouped);
  }

  if (!req->callbackInvoked) {
    uint64_t ns = Latency_End(&commitLatency);
    req->ccb(result);
    req->callbackInvoked = true;
  }
  this->pendingReqs.erase(req->id);
  delete req;
}

bool Client::IsParticipant(int g) const {
  for (const auto &participant : txn.involved_groups()) {
    if (participant == g) {
      return true;
    }
  }
  return false;
}

void Client::Abort(abort_callback acb, abort_timeout_callback atcb,
    uint32_t timeout) {
  transport->Timer(0, [this, acb, atcb, timeout]() {
    // presumably this will be called with empty callbacks as the application can
    // immediately move on to its next transaction without waiting for confirmation
    // that this transaction was aborted

    uint64_t ns = Latency_End(&executeLatency);

    Debug("ABORT[%lu:%lu]", client_id, client_seq_num);

    for (auto group : txn.involved_groups()) {
      bclient[group]->Abort(client_seq_num, txn.timestamp());
    }

    // TODO: can we just call callback immediately?
    acb();
  });
}
///////////////////////// Fallback logic starts here ///////////////////////////////////////////////////////////////

bool Client::isDep(std::string &txnDigest, proto::Transaction &Req_txn){

  for(auto & dep: Req_txn.deps()){
   if(dep.write().prepared_txn_digest() == txnDigest){ return true;}
  }
  return false;
}


void Client::RelayP1callback(proto::RelayP1 &relayP1){ //schedules Phase1FB
  //TODO: check if req.id referenced is still ongoing.
  if(pendingReqs.find(relayP1.conflict_id()) == pendingReqs.end()){return; }

  uint64_t reqID = relayP1.conflict_id();
  proto::Phase1 p1 = relayP1.p1();
   //TODO: Check if the current pending request has this txn as dependency.
  std::string txnDigest = TransactionDigest(p1.txn(), params.hashDigest);
  proto::Transaction Req_txn = pendingReqs[relayP1.conflict_id()]->txn;
  if(!isDep(txnDigest, Req_txn)) return;

// TODO: schedule again for end of timeout. If still valid, then start P1FB

//only relevant if our ongoing txn has not finished P1.
if(pendingReqs[reqID]->outstandingPhase1s > 0){
  struct timeval tv;
  gettimeofday(&tv, NULL);
  uint64_t current_time = (tv.tv_sec*1000000+tv.tv_usec)/1000;
  uint64_t elapsed = current_time - pendingReqs_starttime[reqID];

  if(elapsed > CLIENTTIMEOUT){
    Phase1FB(p1, reqID);
  }
  else{
    transport->Timer((CLIENTTIMEOUT-elapsed), [this, &relayP1](){RelayP1callback(relayP1);});
    return;
  }

}



}
void Client::Phase1FB(proto::Phase1 &p1, uint64_t conflict_id){  //passes callbacks
  std::string txnDigest = TransactionDigest(p1.txn(), params.hashDigest);


  PendingRequest* pendingFB = new PendingRequest(p1.req_id()); //Id doesnt really matter here
  FB_instances[txnDigest] = pendingFB;
  pendingFB->txn = p1.txn();
  pendingFB->txnDigest = txnDigest;
  FB_instances[txnDigest] =  pendingFB;


  for (auto group : p1.txn().involved_groups()) {

    //define all the callbacks here: easier to look at.
    proto::Transaction bind_txn = p1.txn();

      //TODO: ADD req id to all, so we can compare if it is still waiting.
      auto p1fbA = std::bind(&Client::Phase1FBcallbackA, this, conflict_id, txnDigest, group, std::placeholders::_1,
          std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
      auto p1fbB = std::bind(&Client::Phase1FBcallbackB, this, conflict_id, txnDigest, group, std::placeholders::_1,
        std::placeholders::_2);
      auto p2fb = std::bind(&Client::Phase2FBcallback, this, conflict_id, txnDigest, group, std::placeholders::_1,
        std::placeholders::_2);
      auto wb = std::bind(&Client::WritebackFBcallback, this, conflict_id, txnDigest, bind_txn, std::placeholders::_1);
      auto invoke = std::bind(&Client::InvokeFBcallback, this, conflict_id, txnDigest, group); //technically only needed at logging shard

      //Client::WritebackFBcallback(uint64_t conflict_id, std::string txnDigest, proto::Transaction &fbtxn, proto::Writeback &wb)

      //proto::Transaction fbtxn = p1.txn();
      //uint64_t fbID = p1.req_id();
      bclient[group]->Phase1FB(p1, txnDigest, p1fbA, p1fbB, p2fb, wb, invoke);
      //
      // bclient[group]->Phase1FB(p1, txnDigest,
      //   std::bind( &Client::Phase1FBcallbackA, this, conflict_id, txnDigest, group, std::placeholders::_1,
      //       std::placeholders::_2, std::placeholders::_3, std::placeholders::_4),
      //       std::bind( &Client::Phase1FBcallbackB, this, conflict_id, txnDigest, group, std::placeholders::_1,
      //         std::placeholders::_2),
      //         std::bind( &Client::Phase2FBcallback, this, conflict_id, txnDigest, group, std::placeholders::_1,
      //           std::placeholders::_2),
      //           std::bind(&Client::WritebackFBcallback, this, conflict_id, txnDigest, p1.txn(), std::placeholders::_1),
      //           std::bind(&Client::InvokeFBcallback, this, conflict_id, txnDigest, group) );
      pendingFB->outstandingPhase1s++;
    }

}

void Client::Phase2FB(PendingRequest *req){ //this is called from the Phase1FB callbacks.
      //TODO: wrap nomal function, but send P2FB message
      proto::Transaction fb_txn = req->txn;

      uint8_t groupIdx = req->txnDigest[0];
      groupIdx = groupIdx % fb_txn.involved_groups_size();
      UW_ASSERT(groupIdx < fb_txn.involved_groups_size());
      int64_t logGroup = fb_txn.involved_groups(groupIdx);
      Debug("PHASE2FB[%lu:%s][%s] logging to group %ld", client_id, req->txnDigest,
          BytesToHex(req->txnDigest, 16).c_str(), logGroup);


//TODO?: EXTEND THIS TO ALSO CHECK FOR P2 REPLIES AS VALID  PROOFS!!!!
//other case:
// check if p2Replies is of size f+1
      if(req->p2Replies.p2replies().size() >= config->f +1){
        req->startedPhase2 = true;
        bclient[logGroup]->Phase2FB(req->id, req->txn, req->txnDigest, req->decision, req->p2Replies);
        return;
      }

//next lines just minimize signature components.
          if (params.validateProofs && params.signedMessages) {
            if (req->decision == proto::ABORT) {
              UW_ASSERT(req->slowAbortGroup >= 0);
              UW_ASSERT(req->p1ReplySigsGrouped.grouped_sigs().find(req->slowAbortGroup) != req->p1ReplySigsGrouped.grouped_sigs().end());
              while (req->p1ReplySigsGrouped.grouped_sigs().size() > 1) {
                auto itr = req->p1ReplySigsGrouped.mutable_grouped_sigs()->begin();
                if (itr->first == req->slowAbortGroup) {
                  itr++;   //skips this group, i.e. deletes all besides this one.
                }
                req->p1ReplySigsGrouped.mutable_grouped_sigs()->erase(itr);
              }
            }

            uint64_t quorumSize = req->decision == proto::COMMIT ?
                SlowCommitQuorumSize(config) : SlowAbortQuorumSize(config);
            for (auto &groupSigs : *req->p1ReplySigsGrouped.mutable_grouped_sigs()) {
              while (static_cast<uint64_t>(groupSigs.second.sigs_size()) > quorumSize) {
                groupSigs.second.mutable_sigs()->RemoveLast();
              }
            }
          }

          req->startedPhase2 = true;
          bclient[logGroup]->Phase2FB(req->id, req->txn, req->txnDigest, req->decision, req->p1ReplySigsGrouped);
}


void Client::WritebackFB(PendingRequest *req){
  //TODO: wrap normal function
  Debug("WRITEBACKFB[%lu:%s]", client_id, req->txnDigest);

  req->startedWriteback = true;


  transaction_status_t result;
  switch (req->decision) {
    case proto::COMMIT: {
      Debug("WRITEBACKFB[%lu][%s] COMMIT.", client_id,
          BytesToHex(req->txnDigest, 16).c_str());
      result = COMMITTED;
      break;
    }
    case proto::ABORT: {
      result = ABORTED_SYSTEM;
      Debug("WRITEBACK[%lu][%s] ABORT.", client_id,
          BytesToHex(req->txnDigest, 16).c_str());
      break;
    }
    default: {
      NOT_REACHABLE();
    }
  }

  for (auto group : req->txn.involved_groups()) {
    bclient[group]->Writeback(req->txn, req->txnDigest,
        req->decision, req->fast, req->conflict, req->p1ReplySigsGrouped,
        req->p2ReplySigsGrouped);
  }
//delete FB instance.
  this->FB_instances.erase(req->txnDigest);
  delete req; //or only req without *?



}

void Client::Phase1FBcallbackA(uint64_t conflict_id, std::string txnDigest, int64_t group,
  proto::CommitDecision decision, bool fast, const proto::CommittedProof &conflict,
  const std::map<proto::ConcurrencyControl::Result, proto::Signatures> &sigs)  {
  if(pendingReqs.find(conflict_id) == pendingReqs.end()){return; }
  if(pendingReqs[conflict_id]->outstandingPhase1s == 0){ return;}
  //check if instance still active.
  if(FB_instances.find(txnDigest) == FB_instances.end()){Debug("FB instances %s already committed or aborted.", txnDigest); return; }

  PendingRequest* req = FB_instances[txnDigest];
  //Check if the current pending request has this txn as dependency.
  proto::Transaction Req_txn = pendingReqs[conflict_id]->txn;
  if(!isDep(txnDigest, Req_txn)) return;

  // do pretty much normal Phase1Callback. Modify it so it calls Phase2FB or WritebackFB.
  Debug("FBPHASE1[%lu:%s] callback decision %d from group %d", client_id, txnDigest, decision, group);

  if (req->startedPhase2 || req->startedWriteback) {
    Debug("Already started Phase2FB/WritebackFB for FB instance %s. Ignoring Phase1"
        " response from group %d.", txnDigest, group);
    return;
  }

  if (decision == proto::ABORT && fast) {
    if (params.validateProofs) {
      req->conflict = conflict;
    }
  }

  if (params.validateProofs && params.signedMessages) {
    if (decision == proto::ABORT && !fast) {
      auto itr = sigs.find(proto::ConcurrencyControl::ABSTAIN);
      UW_ASSERT(itr != sigs.end());
      Debug("Have %d ABSTAIN replies from group %d.", itr->second.sigs_size(),
          group);
      (*req->p1ReplySigsGrouped.mutable_grouped_sigs())[group] = itr->second;
      req->slowAbortGroup = group;
    } else if (decision == proto::COMMIT) {
      auto itr = sigs.find(proto::ConcurrencyControl::COMMIT);
      UW_ASSERT(itr != sigs.end());
      Debug("Have %d COMMIT replies from group %d.", itr->second.sigs_size(),
          group);
      (*req->p1ReplySigsGrouped.mutable_grouped_sigs())[group] = itr->second;
    }
  }
  if (fast && decision == proto::ABORT) {
    req->fast = true;
  } else {
    req->fast = req->fast && fast;
  }

  --req->outstandingPhase1s;
  switch(decision) {
    case proto::COMMIT:
      break;
    case proto::ABORT:
      // abort!
      req->decision = proto::ABORT;
      req->outstandingPhase1s = 0;
      break;
    default:
      break;
  }

  if (req->outstandingPhase1s == 0) {
    FBHandleAllPhase1Received(req);
  }
  //TODO: find mechanism to include this work in InvokeFB too.
}



void Client::FBHandleAllPhase1Received(PendingRequest *req) {
  Debug("FB instance [%s]: All PHASE1's received", req->txnDigest);
  if (req->fast) {
    WritebackFB(req);
  } else {
    // slow path, must log final result to 1 group
    Phase2FB(req);
  }
}


void Client::Phase1FBcallbackB(uint64_t conflict_id, std::string txnDigest, int64_t group,
   proto::CommitDecision decision, proto::P2Replies p2replies){
     // check if conflict transaction still active
     if(pendingReqs.find(conflict_id) == pendingReqs.end()){return; }
     if(pendingReqs[conflict_id]->outstandingPhase1s == 0){ return;}

    //check if instance still active.
    if(FB_instances.find(txnDigest) == FB_instances.end()){Debug("FB instances %s already committed or aborted.", txnDigest); return; }

    PendingRequest* req = FB_instances[txnDigest];
         //TODO: Check if the current pending request has this txn as dependency.
    proto::Transaction Req_txn = pendingReqs[conflict_id]->txn;
    if(!isDep(txnDigest, Req_txn)) return;

    req->decision = decision;
    req->p2Replies = p2replies;
       //Issue P2FB.
    Phase2FB(req);

        //TODO: find a mechanism to include this work in InvokeFB too.


}

void Client::Phase2FBcallback(uint64_t conflict_id, std::string txnDigest, int64_t group,
   proto::CommitDecision decision, const proto::Signatures &p2ReplySigs ){
  //TODO: map to normal Phase2Callback
if(pendingReqs.find(conflict_id) == pendingReqs.end()){Debug("Request id %lu already committed or aborted.", conflict_id); return; }
if(pendingReqs[conflict_id]->outstandingPhase1s == 0){ return;}
  //check if instance still active.
  if(FB_instances.find(txnDigest) == FB_instances.end()){Debug("FB instances %s already committed or aborted.", txnDigest); return; }

  //check that this is actually a dependent
  proto::Transaction Req_txn = pendingReqs[conflict_id]->txn;
  if(!isDep(txnDigest, Req_txn)) return;

  Debug("PHASE2FB[%lu:%s] callback", client_id, txnDigest);

  PendingRequest* req = FB_instances[txnDigest];

  if (req->startedWriteback) {
      Debug("Already started WritebackFB for FB request id %s. Ignoring Phase2FB response.",
               txnDigest);
          return;
  }
  req->decision = decision;

  if (params.validateProofs && params.signedMessages) {
    (*req->p2ReplySigsGrouped.mutable_grouped_sigs())[group] = p2ReplySigs;
  }

  WritebackFB(req);
}


void Client::WritebackFBcallback(uint64_t conflict_id, std::string txnDigest, proto::Transaction &fbtxn, proto::Writeback &wb) {
  if(pendingReqs.find(conflict_id) == pendingReqs.end()){Debug("Request id %lu already committed or aborted.", conflict_id); return; }
  if(pendingReqs[conflict_id]->outstandingPhase1s == 0){ return;}

  //check if instance still active.
  if(FB_instances.find(txnDigest) == FB_instances.end()){Debug("FB instances %s already committed or aborted.", txnDigest); return; }

  FB_instances[txnDigest]->startedWriteback = true;


  proto::Transaction Req_txn = pendingReqs[conflict_id]->txn;
  if(!isDep(txnDigest, Req_txn)) return;

 for (auto group : fbtxn.involved_groups()) {
   bclient[group]->WritebackFB(txnDigest, wb);
 }
 //delete FB instance.
 auto itr = FB_instances.find(txnDigest);
 delete itr->second;
 FB_instances.erase(txnDigest);

}

void Client::InvokeFBcallback(uint64_t conflict_id, std::string txnDigest, int64_t group){
  //Just send InvokeFB request to the logging shard. but only if the tx has not already finished. and only if we have already sent a P2
  //Otherwise, Include the P2 here!.
  if(pendingReqs.find(conflict_id) == pendingReqs.end()){Debug("Request id %lu already committed or aborted.", conflict_id); return; }
  if(pendingReqs[conflict_id]->outstandingPhase1s == 0){ return;}
  //check if instance still active.
  if(FB_instances.find(txnDigest) == FB_instances.end()){Debug("FB instances %s already committed or aborted.", txnDigest); return; }

  proto::Transaction Req_txn = pendingReqs[conflict_id]->txn;
  if(!isDep(txnDigest, Req_txn)) return;

  PendingRequest* req = FB_instances[txnDigest];
  if(req->startedWriteback){Debug("No p2 decision included - invalid"); return;}
  //we know this group is the FB group, only that group would have invoked this callback.
  bclient[group]->InvokeFB(conflict_id, txnDigest, req->txn, req->decision, req->p2Replies);
}

//void Phase2FB: passes callbacks
//void InvokeFB: calls Phase2FB and adds that to the InvokeFB message.
//void Phase2FBcallback: call either WritebackFB, or InvokeFB




} // namespace indicusstore
