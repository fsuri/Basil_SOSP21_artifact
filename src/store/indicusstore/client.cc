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

#include "store/indicusstore/localbatchverifier.h"
#include "store/indicusstore/basicverifier.h"
#include "store/indicusstore/common.h"
#include <sys/time.h>

namespace indicusstore {

using namespace std;

//TODO: add argument for p1Timeout, pass down to Shardclient as well.
Client::Client(transport::Configuration *config, uint64_t id, int nShards,
    int nGroups,
    const std::vector<int> &closestReplicas, bool pingReplicas, Transport *transport,
    Partitioner *part, bool syncCommit, uint64_t readMessages,
    uint64_t readQuorumSize, Parameters params,
    KeyManager *keyManager, uint64_t phase1DecisionTimeout, TrueTime timeServer)
    : config(config), client_id(id), nshards(nShards), ngroups(nGroups),
    transport(transport), part(part), syncCommit(syncCommit), pingReplicas(pingReplicas),
    readMessages(readMessages), readQuorumSize(readQuorumSize),
    params(params),
    keyManager(keyManager),
    timeServer(timeServer), first(true), startedPings(false),
    client_seq_num(0UL), lastReqId(0UL), getIdx(0UL),
    failureEnabled(false), failureActive(false) {

  Debug("Initializing Indicus client with id [%lu] %lu", client_id, nshards);
  std::cerr<< "P1 Decision Timeout: " <<phase1DecisionTimeout<< std::endl;

  if (params.signatureBatchSize == 1) {
    verifier = new BasicVerifier(transport);//transport, 1000000UL,false); //Need to change interface so client can use it too?
  } else {
    verifier = new LocalBatchVerifier(params.merkleBranchFactor, dummyStats, transport);
  }

  /* Start a client for each shard. */
  for (uint64_t i = 0; i < ngroups; i++) {
    bclient.push_back(new ShardClient(config, transport, client_id, i,
        closestReplicas, pingReplicas, params,
        keyManager, verifier, timeServer, phase1DecisionTimeout));
  }

  Debug("Indicus client [%lu] created! %lu %lu", client_id, nshards,
      bclient.size());
  _Latency_Init(&executeLatency, "execute");
  _Latency_Init(&getLatency, "get");
  _Latency_Init(&commitLatency, "commit");

  if (params.injectFailure.enabled) {
    transport->Timer(params.injectFailure.timeMs, [this](){
        failureEnabled = true;
        // TODO: restore the client after it stalls from phase2_callback from previous txn
      });
  }

  // struct timeval tv;
  // gettimeofday(&tv, NULL);
  // start_time = (tv.tv_sec*1000000+tv.tv_usec)/1000;  //in miliseconds
  //
  // transport->Timer(9500, [this](){
  //   std::cerr<< "experiment about to elapse 10 seconds";
  // });
}

Client::~Client()
{
  //std::cerr << "total failure injections: " << total_failure_injections << std::endl;
  //std::cerr << "total writebacks: " << total_writebacks << std::endl;
  //std::cerr<< "total prepares: " << total_counter << std::endl;
  //std::cerr<< "fast path prepares: " << fast_path_counter << std::endl;
  Latency_Dump(&executeLatency);
  Latency_Dump(&getLatency);
  Latency_Dump(&commitLatency);
  for (auto b : bclient) {
      delete b;
  }
  delete verifier;
}

/* Begins a transaction. All subsequent operations before a commit() or
 * abort() are part of this transaction.
 */
void Client::Begin(begin_callback bcb, begin_timeout_callback btcb,
      uint32_t timeout) {
  // fail the current txn iff failuer timer is up and
  // the number of txn is a multiple of frequency
  failureActive = failureEnabled &&
    (client_seq_num % params.injectFailure.frequency == 0);
  for (auto b : bclient) {
    b->SetFailureFlag(failureActive);
  }

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
    //std::cerr<< "client_seq_num: " << client_seq_num << std::endl;
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

    //std::cerr << "value size: " << value.size() << "; key " << BytesToHex(key,16).c_str() << std::endl;

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

    //XXX flag to sort read/write sets for parallel OCC
    if(params.parallel_CCC){
      std::sort(txn.mutable_read_set()->begin(), txn.mutable_read_set()->end(), sortReadByKey);
      std::sort(txn.mutable_write_set()->begin(), txn.mutable_write_set()->end(), sortWriteByKey);
    }

    PendingRequest *req = new PendingRequest(client_seq_num, this);
    pendingReqs[client_seq_num] = req;
    req->txn = txn; //Is this a copy or just reference?
    req->ccb = ccb;
    req->ctcb = ctcb;
    req->callbackInvoked = false;
    req->txnDigest = TransactionDigest(txn, params.hashDigest);
    req->timeout = timeout;
    stats.IncrementList("txn_groups", txn.involved_groups().size());

    Phase1(req);
  });
}

void Client::Phase1(PendingRequest *req) {
  Debug("PHASE1 [%lu:%lu] at %lu", client_id, client_seq_num,
      txn.timestamp().timestamp());

  UW_ASSERT(txn.involved_groups().size() > 0);

  for (auto group : txn.involved_groups()) {
    bclient[group]->Phase1(client_seq_num, txn, req->txnDigest,
        std::bind( &Client::Phase1Callback, this, req->id, group, std::placeholders::_1,
          std::placeholders::_2, std::placeholders::_3,
          std::placeholders::_4, std::placeholders::_5, std::placeholders::_6),
        std::bind(&Client::Phase1TimeoutCallback, this, group, req->id,
          std::placeholders::_1),
        std::bind(&Client::RelayP1callback, this, req->id, std::placeholders::_1, std::placeholders::_2),
        req->timeout);
    req->outstandingPhase1s++;
  }
  //schedule timeout for when we allow starting FB P1.
  transport->Timer(params.relayP1_timeout, [this, reqId = req->id](){RelayP1TimeoutCallback(reqId);});

}



void Client::Phase1CallbackProcessing(PendingRequest *req, int group,
    proto::CommitDecision decision, bool fast, bool conflict_flag,
    const proto::CommittedProof &conflict,
    const std::map<proto::ConcurrencyControl::Result, proto::Signatures> &sigs,
    bool eqv_ready){

  if (decision == proto::ABORT && fast && conflict_flag) {
      req->conflict = conflict;
      req->conflict_flag = true;
  }

  if (params.validateProofs && params.signedMessages) {
    if (eqv_ready) {
      // saving abort sigs in req->eqvAbortSigsGrouped
      auto itr_abort = sigs.find(proto::ConcurrencyControl::ABSTAIN);
      UW_ASSERT(itr_abort != sigs.end());
      Debug("Have %d ABSTAIN replies from group %d. Saving in eqvAbortSigsGrouped",
          itr_abort->second.sigs_size(), group);
      (*req->eqvAbortSigsGrouped.mutable_grouped_sigs())[group] = itr_abort->second;

      // saving commit sigs in req->p1ReplySigsGrouped
      auto itr_commit = sigs.find(proto::ConcurrencyControl::COMMIT);
      UW_ASSERT(itr_commit != sigs.end());
      Debug("Have %d COMMIT replies from group %d.", itr_commit->second.sigs_size(),
          group);
      (*req->p1ReplySigsGrouped.mutable_grouped_sigs())[group] = itr_commit->second;

    }
    //non-equivocation processing
    else if(decision == proto::ABORT && fast && !conflict_flag){
      auto itr = sigs.find(proto::ConcurrencyControl::ABSTAIN);
      UW_ASSERT(itr != sigs.end());
      Debug("Have %d ABSTAIN replies from group %d.", itr->second.sigs_size(),
          group);
      (*req->p1ReplySigsGrouped.mutable_grouped_sigs())[group] = itr->second;
      req->fastAbortGroup = group;
    } else if (decision == proto::ABORT && !fast) {
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
  if (eqv_ready) req->eqv_ready = true;

  --req->outstandingPhase1s;
  switch(decision) {
    case proto::COMMIT:
      break; //decision is proto::COMMIT by default
    case proto::ABORT:
      // abort!
      req->decision = proto::ABORT;
      req->eqv_ready = false; //if there is a single shard that is only abort, then equiv not possible
      req->outstandingPhase1s = 0;
      break;
    default:
      break;
  }
}

void Client::Phase1Callback(uint64_t txnId, int group,
    proto::CommitDecision decision, bool fast, bool conflict_flag,
    const proto::CommittedProof &conflict,
    const std::map<proto::ConcurrencyControl::Result, proto::Signatures> &sigs,
    bool eqv_ready) {
  auto itr = this->pendingReqs.find(txnId);
  if (itr == this->pendingReqs.end()) {
    Debug("Phase1Callback for terminated request %lu (txn already committed"
        " or aborted.", txnId);
    return;
  }

  //total_counter++;
  //if(fast) fast_path_counter++;
  stats.Increment("total_prepares", 1);
  if(fast) stats.Increment("total_prepares_fast", 1);

  Debug("PHASE1[%lu:%lu] callback decision %d [Fast:%s][Conflict:%s] from group %d", client_id,
      client_seq_num, decision, fast ? "yes" : "no", conflict_flag ? "yes" : "no", group);

  PendingRequest *req = itr->second;

  if (req->startedPhase2 || req->startedWriteback) {
    Debug("Already started Phase2/Writeback for request id %lu. Ignoring Phase1"
        " response from group %d.", txnId, group);
    return;
  }

  Phase1CallbackProcessing(req, group, decision, fast, conflict_flag, conflict, sigs, eqv_ready);

  if (req->outstandingPhase1s == 0) {
    HandleAllPhase1Received(req);
  }
    //XXX use StopP1 to shortcircuit all shard clients
  //bclient[group]->StopP1(txnId);
}


void Client::Phase1TimeoutCallback(int group, uint64_t txnId, int status) {
  auto itr = this->pendingReqs.find(txnId);
  if (itr == this->pendingReqs.end()) {
    return;
  }

  return;  //TODO:: REMOVE AND REPLACE

  PendingRequest *req = itr->second;
  if (req->startedPhase2 || req->startedWriteback) {
    return;
  }

  Warning("PHASE1[%lu:%lu] group %d timed out.", client_id, txnId, group);

  req->outstandingPhase1s = 0;
  if (params.validateProofs && params.signedMessages) {
    req->slowAbortGroup = -1;
    req->p1ReplySigsGrouped.mutable_grouped_sigs()->clear();
    req->fast = true;
    req->decision = proto::COMMIT;
  }
  Phase1(req);

  //TODO:: alternatively upon timeout: just start Phase1FB for ones own TX:
  //Todo so: shard client needs to upcall with the respective reqId from shard client.. re-create the p1 message.
  // proto::Phase1 *p1 = new proto::Phase1();
  // p1->set_req_id(reqId); //TODO: probably can remove this reqId again.
  // *p1->mutable_txn()= req->txn;
  // Phase1FB(p1, txnId, req->txnDigest);
}

void Client::HandleAllPhase1Received(PendingRequest *req) {
  Debug("All PHASE1's [%lu] received", client_seq_num);
  //TO force P2, add "req->conflict_flag". Conflict Aborts *must* go fast path.
  if (req->fast) {
    Writeback(req);
  } else {
    // slow path, must log final result to 1 group
    if (req->eqv_ready) {
      Phase2Equivocate(req);
    } else {
      Phase2(req);
    }
  }
}

void Client::Phase2Processing(PendingRequest *req){

  if (params.validateProofs && params.signedMessages) {
    if (req->decision == proto::ABORT) {
      UW_ASSERT(req->slowAbortGroup >= 0);
      UW_ASSERT(req->p1ReplySigsGrouped.grouped_sigs().find(req->slowAbortGroup) != req->p1ReplySigsGrouped.grouped_sigs().end());
      while (req->p1ReplySigsGrouped.grouped_sigs().size() > 1) {
        auto itr = req->p1ReplySigsGrouped.mutable_grouped_sigs()->begin();
        if (itr->first == req->slowAbortGroup) {
          itr++;  //skips this group, i.e. deletes all besides this one.
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
}

void Client::Phase2(PendingRequest *req) {
  int64_t logGroup = GetLogGroup(txn, req->txnDigest);

  Debug("PHASE2[%lu:%lu][%s] logging to group %ld", client_id, client_seq_num,
      BytesToHex(req->txnDigest, 16).c_str(), logGroup);

  Phase2Processing(req);

  //Simulating equiv failures, only if current decision is Commit
  if(failureActive && params.injectFailure.type == InjectFailureType::CLIENT_EQUIVOCATE_SIMULATE
       && req->decision == proto::COMMIT){
    bclient[logGroup]->Phase2Equivocate_Simulate(client_seq_num, txn, req->txnDigest,
        req->p1ReplySigsGrouped);
    
    std::cerr << "SIMULATED EQUIVOCATION. STOPPING" << std::endl;
    //terminate ongoing tx mangagement and move to next tx:
    FailureCleanUp(req);
    return;
  }

  bclient[logGroup]->Phase2(client_seq_num, txn, req->txnDigest, req->decision,
      req->p1ReplySigsGrouped,
      std::bind(&Client::Phase2Callback, this, req->id, logGroup,
        std::placeholders::_1),
      std::bind(&Client::Phase2TimeoutCallback, this, logGroup, req->id,
        std::placeholders::_1), req->timeout);

    //XXX use StopP1 to shortcircuit all shard clients
  // for(auto group : req->txn.involved_groups()){
  //     bclient[group]->StopP1(req->id);
  // }
}

void Client::Phase2Equivocate(PendingRequest *req) {
  int64_t logGroup = GetLogGroup(txn, req->txnDigest);

  Debug("PHASE2[%lu:%lu][%s] logging to group %ld with equivocation", client_id, client_seq_num,
      BytesToHex(req->txnDigest, 16).c_str(), logGroup);

  if (params.validateProofs && params.signedMessages) {
    // build grouped commits sigs with size of commitQuorum
    for (auto &groupSigs : *req->p1ReplySigsGrouped.mutable_grouped_sigs()) {
      while (static_cast<uint64_t>(groupSigs.second.sigs_size()) > SlowCommitQuorumSize(config)) {
        groupSigs.second.mutable_sigs()->RemoveLast();
      }
    }

    // build grouped abort sigs with size of abortQuorum
    for (auto &groupSigs : *req->eqvAbortSigsGrouped.mutable_grouped_sigs()) {
      while (static_cast<uint64_t>(groupSigs.second.sigs_size()) > SlowAbortQuorumSize(config)) {
        groupSigs.second.mutable_sigs()->RemoveLast();
      }
    }
  }

  req->startedPhase2 = true;
  bclient[logGroup]->Phase2Equivocate(client_seq_num, txn, req->txnDigest,
      req->p1ReplySigsGrouped, req->eqvAbortSigsGrouped);
  //NOTE DONT need to use version with callbacks... callbacks are obsolete if we return directly.
  // bclient[logGroup]->Phase2Equivocate(client_seq_num, txn, req->txnDigest,
  //     req->p1ReplySigsGrouped, req->eqvAbortSigsGrouped,
  //     std::bind(&Client::Phase2Callback, this, req->id, logGroup,
  //       std::placeholders::_1),
  //     std::bind(&Client::Phase2TimeoutCallback, this, logGroup, req->id,
  //       std::placeholders::_1), req->timeout);

  //terminate ongoing tx mangagement and move to next tx:
  FailureCleanUp(req);
}

void Client::Phase2Callback(uint64_t txnId, int group,
    const proto::Signatures &p2ReplySigs) {
  auto itr = this->pendingReqs.find(txnId);
  if (itr == this->pendingReqs.end()) {
    Debug("Phase2Callback for terminated request id %lu (txn already committed or aborted).", txnId);
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

void Client::WritebackProcessing(PendingRequest *req){

  //////Block to handle Fast Abort case with no conflict: Reduce groups sent... and total replies sent
  if (params.validateProofs && params.signedMessages) {
    if (req->decision == proto::ABORT && req->fast && !req->conflict_flag) {
      UW_ASSERT(req->fastAbortGroup >= 0);
      UW_ASSERT(req->p1ReplySigsGrouped.grouped_sigs().find(req->fastAbortGroup) != req->p1ReplySigsGrouped.grouped_sigs().end());
      while (req->p1ReplySigsGrouped.grouped_sigs().size() > 1) {
        auto itr = req->p1ReplySigsGrouped.mutable_grouped_sigs()->begin();
        if (itr->first == req->fastAbortGroup) {
          itr++;
        }
        req->p1ReplySigsGrouped.mutable_grouped_sigs()->erase(itr);
      }

      uint64_t quorumSize = FastAbortQuorumSize(config);
      for (auto &groupSigs : *req->p1ReplySigsGrouped.mutable_grouped_sigs()) {
        while (static_cast<uint64_t>(groupSigs.second.sigs_size()) > quorumSize) {
          groupSigs.second.mutable_sigs()->RemoveLast();
        }
      }
     }
  }
}

void Client::Writeback(PendingRequest *req) {

  //total_writebacks++;
  Debug("WRITEBACK[%lu:%lu] result %s", client_id, req->id, req->decision ?  "ABORT" : "COMMIT");

  req->startedWriteback = true;

  if (failureActive && params.injectFailure.type == InjectFailureType::CLIENT_CRASH) {
    Debug("INJECT CRASH FAILURE[%lu:%lu] with decision %d. txnDigest: %s", client_id, req->id, req->decision,
          BytesToHex(TransactionDigest(req->txn, params.hashDigest), 16).c_str());
    //stats.Increment("inject_failure_crash");
    //total_failure_injections++;
    FailureCleanUp(req);
    return;
  }


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
  //this function truncates sigs for the Abort p1 fast case
  WritebackProcessing(req);

  for (auto group : txn.involved_groups()) {
    bclient[group]->Writeback(client_seq_num, txn, req->txnDigest,
        req->decision, req->fast, req->conflict_flag, req->conflict, req->p1ReplySigsGrouped,
        req->p2ReplySigsGrouped);
  }

  if (!req->callbackInvoked) {
    uint64_t ns = Latency_End(&commitLatency);
    req->ccb(result);
    req->callbackInvoked = true;
  }
  //XXX use StopP1 to shortcircuit all shard clients
  // for(auto group : req->txn.involved_groups()){
  //   bclient[group]->StopP1(req->id);
  // }

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

// for crash, report the result of p1
// for equivocation, always report ABORT, always delete
void Client::FailureCleanUp(PendingRequest *req) {
  UW_ASSERT(failureActive);
  transaction_status_t result;
  Debug("FailureCleanUp[%lu:%lu] for type[%s]", client_id, req->id,
    params.injectFailure.type == InjectFailureType::CLIENT_CRASH ? "CRASH" : "EQUIVOCATE");
  if (params.injectFailure.type == InjectFailureType::CLIENT_CRASH) {
    if (req->decision == proto::COMMIT) {
      result = COMMITTED;
    } else {
      result = ABORTED_SYSTEM;
    }
  } else {
    // alaways report ABORT for equivocation
    result = ABORTED_SYSTEM;
  }
  if (!req->callbackInvoked) {
    uint64_t ns = Latency_End(&commitLatency);
    req->ccb(result);
    req->callbackInvoked = true;
  }
  this->pendingReqs.erase(req->id);
  delete req;
}


//INSTEAD: just use the existing ForwardWriteback function from the FB, and then add ongoing txn to it..
void Client::ForwardWBcallback(uint64_t txnId, int group, proto::ForwardWriteback &forwardWB){
  auto itr = this->pendingReqs.find(txnId);
  if (itr == this->pendingReqs.end()) {
    Debug("ForwardWBcallback for terminated request id %lu (txn already committed or aborted).", txnId);
    return;
  }
  PendingRequest *req = itr->second;

  if (req->startedWriteback) {
    Debug("Already started Writeback for request id %lu. Ignoring Phase2 response.",
        txnId);
    return;
  }

  req->startedWriteback = true;
  req->decision = forwardWB.decision();
  if(forwardWB.has_p1_sigs()){
    req->fast = true;
    req->p1ReplySigsGrouped.Swap(forwardWB.mutable_p1_sigs());
  }
  else if(forwardWB.has_p2_sigs()){
    req->fast = false;
    req->p2ReplySigsGrouped.Swap(forwardWB.mutable_p2_sigs());
  }
  else if(forwardWB.has_conflict()){
    req->conflict_flag = true;
    req->conflict.Swap(forwardWB.mutable_conflict());
  }
  else{
    Panic("ForwardWB message has no proofs");
  }

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
        req->decision, req->fast, req->conflict_flag, req->conflict, req->p1ReplySigsGrouped,
        req->p2ReplySigsGrouped);
  }

  if (!req->callbackInvoked) {
    uint64_t ns = Latency_End(&commitLatency);
    req->ccb(result);
    req->callbackInvoked = true;
  }

}
///////////////////////// Fallback logic starts here ///////////////////////////////////////////////////////////////

bool Client::isDep(const std::string &txnDigest, proto::Transaction &Req_txn){

  for(auto & dep: Req_txn.deps()){
   if(dep.write().prepared_txn_digest() == txnDigest){ return true;}
  }
  return false;
}



bool Client::StillActive(uint64_t conflict_id, std::string &txnDigest){
  //check if FB instance is (still) active.
  auto itr = FB_instances.find(txnDigest);
  if(itr == FB_instances.end()){
    Debug("FB instances %s already committed or aborted.", txnDigest.c_str());
    return false;
  }

  return true; //Altruism
  //TODO: Can be altruistic and finish FB instance even if not blocking.
  auto itrReq = pendingReqs.find(conflict_id);
  if(itrReq == pendingReqs.end() || itrReq->second->outstandingPhase1s == 0){
    Debug("Request id %lu already committed, aborted, or in the process of doing so.", conflict_id);
    CleanFB(itr->second, txnDigest); //call into Shard clients to clean up state as well!
    return false;
  }

  //check if we have a dependent (has_dependent), and whether it is done? If so, call cleanCB()
  if(itr->second->has_dependent && FB_instances.find(itr->second->dependent) == FB_instances.end()){
    Debug("Dependent of txn[%s] already committed, aborted, or in the process of doing so.", BytesToHex(txnDigest, 64).c_str());
    CleanFB(itr->second, txnDigest); //call into Shard clients to clean up state as well!
    return false;
  }

  return true;
}

void Client::CleanFB(PendingRequest *pendingFB, const std::string &txnDigest){
  Debug("Called CleanFB for txnDigest: %s ", BytesToHex(txnDigest, 64).c_str());
  for(auto group : pendingFB->txn.involved_groups()){
    Debug("Cleaned shard client: %d", group);
    bclient[group]->CleanFB(txnDigest);
  } //TODO: this is not actually necessary if it is part of pendingFB destructor...
  FB_instances.erase(txnDigest);
  delete pendingFB;
}

void Client::EraseRelays(proto::RelayP1 &relayP1, std::string &txnDigest){
  for(auto group : relayP1.p1().txn().involved_groups()){
    bclient[group]->EraseRelay(txnDigest);
  }
}

void Client::RelayP1callback(uint64_t reqId, proto::RelayP1 &relayP1, std::string& txnDigest){

  auto itr = pendingReqs.find(reqId);
  if(itr == pendingReqs.end()){
    Debug("ReqId[%d] has already completed", reqId);
    //EraseRelays(relayP1, txnDigest);
    return;
  }

  //const std::string &txnDigest = TransactionDigest(p1->txn(), params.hashDigest);

  //do not start multiple FB instances for the same TX
  //if(itr->second->req_FB_instances.find(txnDigest) != itr->second->req_FB_instances.end()) return;
  if(Completed_transactions.find(txnDigest) != Completed_transactions.end()) return;
  if(FB_instances.find(txnDigest) != FB_instances.end()) return;
  //Check if the current pending request has this txn as dependency.
  if(!isDep(txnDigest, itr->second->txn)){
    Debug("Tx[%s] is not a dependency of ReqId: %d", BytesToHex(txnDigest, 16).c_str(), reqId);
    return;
  }

  proto::Phase1 *p1 = relayP1.release_p1();

  if(itr->second->startFB){
    Debug("Starting Phase1FB directly for txn: %s", BytesToHex(txnDigest, 16).c_str());
    //itr->second->req_FB_instances.insert(txnDigest); //XXX mark connection between reqId and FB instance
    Phase1FB(txnDigest, reqId, p1);
  }
  else{
    Debug("Adding to FB buffer for txn: %s", BytesToHex(txnDigest, 16).c_str());
    //itr->second->RelayP1s.emplace_back(p1, txnDigest);
    itr->second->RelayP1s[txnDigest] = p1;
  }
}

void Client::RelayP1TimeoutCallback(uint64_t reqId){
  auto itr = pendingReqs.find(reqId);
  if(itr == pendingReqs.end()){
    Debug("ClientSeqNum[%d] has already completed", reqId);
    return;
  }

  itr->second->startFB = true;
  for(auto p1_pair : itr->second->RelayP1s){
    Debug("Starting Phase1FB from FB buffer for dependent txnId: %d", reqId);
    //itr->second->req_FB_instances.insert(p1_pair.first); //XXX mark connection between reqId and FB instance
    Phase1FB(p1_pair.first, reqId, p1_pair.second);
  }
  //
}

// Additional Relay FB handler for Fallbacks for dependencies of dependencies.
void Client::RelayP1callbackFB(uint64_t reqId, const std::string &dependent_txnDigest, proto::RelayP1 &relayP1, std::string& txnDigest){
  //dont need to respect a time out here?

  auto itr = pendingReqs.find(reqId);
  if(itr == pendingReqs.end()){
    Debug("ReqId[%d] has already completed", reqId);
    //EraseRelays(relayP1, txnDigest);
    return;
  }

  auto itrFB = FB_instances.find(dependent_txnDigest);
  if(itrFB == FB_instances.end()){
    Debug("FB txn[%s] has already completed", BytesToHex(dependent_txnDigest, 64).c_str());
    //EraseRelays(relayP1, txnDigest);
    return;
  }

  //const std::string &txnDigest = TransactionDigest(p1->txn(), params.hashDigest);

  //do not start multiple FB instances for the same TX
  //if(itr->second->req_FB_instances.find(txnDigest) != itr->second->req_FB_instances.end()) return; //Unlike FB_instances, this set only gets erased upon Request completion
  if(Completed_transactions.find(txnDigest) != Completed_transactions.end()) return;
  if(FB_instances.find(txnDigest) != FB_instances.end()) return;
   //Check if the current pending request has this txn as dependency.
  if(!isDep(txnDigest, itrFB->second->txn)){
    Debug("Tx[%s] is not a dependency of ReqId: %d", BytesToHex(txnDigest, 128).c_str(), reqId);
    return;
  }

  proto::Phase1 *p1 = relayP1.release_p1();

  Debug("Starting Phase1FB for deeper depth right away");

  //itr->second->req_FB_instances.insert(txnDigest); //XXX mark connection between reqId and FB instance
  Phase1FB_deeper(reqId, txnDigest, dependent_txnDigest, p1);
}


void Client::Phase1FB(const std::string &txnDigest, uint64_t conflict_id, proto::Phase1 *p1){  //passes callbacks

  Debug("Started Phase1FB for txn: %s, for dependent ID: %d", BytesToHex(txnDigest, 16).c_str(), conflict_id);

  PendingRequest* pendingFB = new PendingRequest(p1->req_id(), this); //Id doesnt really matter here
  pendingFB->txn = p1->txn();
  pendingFB->txnDigest = txnDigest;
  pendingFB->has_dependent = false;
  FB_instances[txnDigest] = pendingFB;

  SendPhase1FB(p1, conflict_id, txnDigest, pendingFB);

}

void Client::Phase1FB_deeper(uint64_t conflict_id, const std::string &txnDigest, const std::string &dependent_txnDigest, proto::Phase1 *p1){

  Debug("Starting Phase1FB for deeper depth. Original conflict id: %d, Dependent txnDigest: %s, txnDigest of tx causing the stall %s", conflict_id, BytesToHex(dependent_txnDigest, 16).c_str(), BytesToHex(txnDigest, 16).c_str());

  PendingRequest* pendingFB = new PendingRequest(p1->req_id(), this); //Id doesnt really matter here
  pendingFB->txn = p1->txn();
  pendingFB->txnDigest = txnDigest;
  pendingFB->has_dependent = true;
  pendingFB->dependent = dependent_txnDigest;
  FB_instances[txnDigest] = pendingFB;
  SendPhase1FB(p1, conflict_id, txnDigest, pendingFB);
}

void Client::SendPhase1FB(proto::Phase1 *p1, uint64_t conflict_id, const std::string &txnDigest, PendingRequest *pendingFB){

  for (auto group : p1->txn().involved_groups()) {
      Debug("Client %d, Send Phase1FB for txn %s to involved group %d", client_id, BytesToHex(txnDigest, 16).c_str(), group);

    //define all the callbacks here
      //bind conflict_id (i.e. the top level dependent) to all, so we can check if it is still waiting.
      //TODO: dont bind it but make it a field of the PendingRequest..
      auto p1Relay = std::bind(&Client::RelayP1callbackFB, this, conflict_id, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

      auto p1fbA = std::bind(&Client::Phase1FBcallbackA, this, conflict_id, txnDigest, group, std::placeholders::_1,
          std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);
      auto p1fbB = std::bind(&Client::Phase1FBcallbackB, this, conflict_id, txnDigest, group, std::placeholders::_1,
        std::placeholders::_2);
      auto p2fb = std::bind(&Client::Phase2FBcallback, this, conflict_id, txnDigest, group, std::placeholders::_1,
        std::placeholders::_2, std::placeholders::_3);
      auto wb = std::bind(&Client::WritebackFBcallback, this, conflict_id, txnDigest, std::placeholders::_1);
      auto invoke = std::bind(&Client::InvokeFBcallback, this, conflict_id, txnDigest, group); //technically only needed at logging shard

      //bclient[group]->Phase1FB(p1->req_id(), pendingFB->txn, txnDigest, p1Relay, p1fbA, p1fbB, p2fb, wb, invoke);
      bclient[group]->Phase1FB(client_id, pendingFB->txn, txnDigest, p1Relay, p1fbA, p1fbB, p2fb, wb, invoke);

      pendingFB->outstandingPhase1s++;
    }
  delete p1;
  Debug("Sent all Phase1FB for txn[%s]", BytesToHex(txnDigest, 64).c_str());
  return;
}



//If Receive a finished FB just forward it.
void Client::WritebackFBcallback(uint64_t conflict_id, std::string txnDigest, proto::Writeback &wb) {
  Debug("Received WritebackFB callback fast for txn: %s", BytesToHex(txnDigest, 16).c_str());

  if(!StillActive(conflict_id, txnDigest)) return;
  auto itr = FB_instances.find(txnDigest);
  PendingRequest *pendingFB = itr->second;
  if(pendingFB->startedWriteback) return;
  pendingFB->startedWriteback = true;

  Debug("Forwarding WritebackFB fast for txn: %s",BytesToHex(txnDigest, 16).c_str());
  //TODO: Need to validate WB message:
  // 1) check that txnDigest matches txn content
  // 2) check that sigs match decision and txnDigest
  //TODO:: CHECK THAT fbtxn matches digest? Not necessary if we just set the contents ourselves.
  // set txn field here.
  //Also: server side message might not include txn, hence include it ourselves just in case.

  *wb.mutable_txn() = std::move(pendingFB->txn);

 for (auto group : wb.txn().involved_groups()) {
   bclient[group]->WritebackFB_fast(txnDigest, wb);
 }
 //delete FB instance. (doing so early will make sure other ShardClients dont waste work.)
 Completed_transactions.insert(txnDigest);
 CleanFB(pendingFB, txnDigest);
}



void Client::Phase1FBcallbackA(uint64_t conflict_id, std::string txnDigest, int64_t group,
  proto::CommitDecision decision, bool fast, bool conflict_flag,
  const proto::CommittedProof &conflict,
  const std::map<proto::ConcurrencyControl::Result, proto::Signatures> &sigs)  {

  Debug("Phase1FBcallbackA called for txn[%s] with decision: %d", BytesToHex(txnDigest, 64).c_str(), decision);

  if(!StillActive(conflict_id, txnDigest)) return;

  PendingRequest* req = FB_instances[txnDigest];

  if (req->startedPhase2 || req->startedWriteback) {
    Debug("Already started Phase2FB/WritebackFB for tx [%s[]. Ignoring Phase1 callback"
        " response from group %d.", BytesToHex(txnDigest, 128).c_str(), group);
    return;
  }

  Debug("Processing Phase1CallbackA for txn: %s with decision %d", BytesToHex(txnDigest, 16).c_str(), decision);
  Phase1CallbackProcessing(req, group, decision, fast, conflict_flag, conflict, sigs);

  if (req->outstandingPhase1s == 0) {
    FBHandleAllPhase1Received(req);
  }
  //TODO: find mechanism to include this work in InvokeFB too.
}



void Client::FBHandleAllPhase1Received(PendingRequest *req) {
  Debug("FB instance [%s]: All PHASE1's received", req->txnDigest.c_str());
  if (req->fast) {
    WritebackFB(req);
  } else {
    // slow path, must log final result to 1 group
    Phase2FB(req);
  }
}


//XXX cannot just send decision, but need to send whole p2 replies because the decision views might differ
//making it of type bool so that shard client does not require req Id dependency
bool Client::Phase1FBcallbackB(uint64_t conflict_id, std::string txnDigest, int64_t group,
   proto::CommitDecision decision, const proto::P2Replies &p2replies){

     Debug("Phase1FBcallbackB called for txn[%s] with decision %d", BytesToHex(txnDigest, 64).c_str(), decision);

     // check if conflict transaction still active
    if(!StillActive(conflict_id, txnDigest)) return false;

    PendingRequest* req = FB_instances[txnDigest];

    if (req->startedPhase2 || req->startedWriteback) {
      Debug("Already started Phase2FB/WritebackFB for tx [%s[]. Ignoring Phase1 callback"
          " response from group %d.", BytesToHex(txnDigest, 128).c_str(), group);
      return true;
    }

    req->decision = decision;
    req->p2Replies = std::move(p2replies);
       //Issue P2FB.
    Phase2FB(req);

    return true;
        //TODO: find a mechanism to include this work in InvokeFB too.
}

void Client::Phase2FB(PendingRequest *req){

      Debug("Sending Phase2FB for txn[%s] with decision: %d", BytesToHex(req->txnDigest, 16).c_str(), req->decision);

      const proto::Transaction &fb_txn = req->txn;

      uint8_t groupIdx = req->txnDigest[0];
      groupIdx = groupIdx % fb_txn.involved_groups_size();
      UW_ASSERT(groupIdx < fb_txn.involved_groups_size());
      int64_t logGroup = fb_txn.involved_groups(groupIdx);
      Debug("PHASE2FB[%lu:%s][%s] logging to group %ld", client_id, req->txnDigest.c_str(),
          BytesToHex(req->txnDigest, 16).c_str(), logGroup);

      //CASE THAT CHECKS FOR P2 REPLIES AS VALID  PROOFS
      // check if p2Replies is of size f+1
      if(req->p2Replies.p2replies().size() >= config->f +1){
        req->startedPhase2 = true;
        bclient[logGroup]->Phase2FB(req->id, req->txn, req->txnDigest, req->decision, req->p2Replies);
      }
      else{ //OTHERWISE: Use p1 sigs just like in normal case.
        Phase2Processing(req);

        bclient[logGroup]->Phase2FB(req->id, req->txn, req->txnDigest, req->decision,
          req->p1ReplySigsGrouped);
      }
      for (auto group : req->txn.involved_groups()) {
        bclient[group]->StopP1FB(req->txnDigest);
      }
}

//TODO: extend with view.
void Client::Phase2FBcallback(uint64_t conflict_id, std::string txnDigest, int64_t group,
   proto::CommitDecision decision, const proto::Signatures &p2ReplySigs, uint64_t view){

  Debug("Phase2FBcallback called for txn[%s] with decision %d", BytesToHex(txnDigest, 16).c_str(), decision);
  // check if conflict transaction still active
  if(!StillActive(conflict_id, txnDigest)) return;

  Debug("PHASE2FB[%lu:%s] callback", client_id, txnDigest.c_str());

  PendingRequest* req = FB_instances[txnDigest];

  if (req->startedWriteback) {
      Debug("Already started WritebackFB for FB request id %s. Ignoring Phase2FB response.",
               txnDigest.c_str());
          return;
  }
  req->decision = decision;
  req->decision_view = view;
  req->fast = false;

  if (params.validateProofs && params.signedMessages) {
    (*req->p2ReplySigsGrouped.mutable_grouped_sigs())[group] = p2ReplySigs;
  }

  WritebackFB(req);
}

void Client::WritebackFB(PendingRequest *req){

  Debug("WRITEBACKFB[%lu:%s] result: %s", client_id, BytesToHex(req->txnDigest, 16).c_str(), req->decision ? "COMMIT" : "ABORT");

  req->startedWriteback = true;
  WritebackProcessing(req);

  for (auto group : req->txn.involved_groups()) {
    bclient[group]->Writeback(0, req->txn, req->txnDigest,
        req->decision, req->fast, req->conflict_flag, req->conflict, req->p1ReplySigsGrouped,
        req->p2ReplySigsGrouped, req->decision_view);
  }
  //delete FB instance. (doing so early will make sure other ShardClients dont waste work.)
  Completed_transactions.insert(req->txnDigest);
  CleanFB(req, req->txnDigest);
}

bool Client::InvokeFBcallback(uint64_t conflict_id, std::string txnDigest, int64_t group){
  //Just send InvokeFB request to the logging shard. but only if the tx has not already finished. and only if we have already sent a P2
  //Otherwise, Include the P2 here!.

  // check if conflict transaction still active
  if(!StillActive(conflict_id, txnDigest)) return false;


  //TODO: add flags for P2 sent: If not sent yet, need to wait for that.
  PendingRequest* req = FB_instances[txnDigest];
  if(req->startedWriteback){
    Debug("Already sent WB - unecessary InvokeFB");
    return true;
  }
  //TODO: RECOMMENT. Currently assuming that all servers already have p2 decision.
  if(false && req->p2Replies.p2replies().size() < config->f +1){
    Debug("No p2 decision included - invalid InvokeFB");
    return true;
  }

  Debug("Called InvokeFB on logging shard group %lu, for txn: %s", group, BytesToHex(txnDigest, 16).c_str());
  //we know this group is the FB group, only that group would have invoked this callback.
  bclient[group]->InvokeFB(conflict_id, txnDigest, req->txn, req->decision, req->p2Replies);

return true;
  //TODO: add logic to include a P2 based of P1's.
  // else if (req->outstandingPhase1s == 0) {
  //     req->p1ReplySigsGrouped
  // else return;
}

//void Phase2FB: passes callbacks
//void InvokeFB: calls Phase2FB and adds that to the InvokeFB message.
//void Phase2FBcallback: call either WritebackFB, or InvokeFB




} // namespace indicusstore
