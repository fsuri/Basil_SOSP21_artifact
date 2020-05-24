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
    timeServer(timeServer), failureActive(false), first(true), startedPings(false),
    client_seq_num(0UL), lastReqId(0UL), getIdx(0UL) {

  Debug("Initializing Indicus client with id [%lu] %lu", client_id, nshards);

  if (params.signatureBatchSize == 1) {
    verifier = new BasicVerifier();
  } else {
    verifier = new LocalBatchVerifier(params.merkleBranchFactor, dummyStats);
  }

  /* Start a client for each shard. */
  for (uint64_t i = 0; i < ngroups; i++) {
    bclient.push_back(new ShardClient(config, transport, client_id, i,
        closestReplicas, pingReplicas, params,
        keyManager, verifier, timeServer));
  }

  Debug("Indicus client [%lu] created! %lu %lu", client_id, nshards,
      bclient.size());
  _Latency_Init(&executeLatency, "execute");
  _Latency_Init(&getLatency, "get");
  _Latency_Init(&commitLatency, "commit");

  if (params.injectFailure.enabled) {
    transport->Timer(params.injectFailure.timeMs, [this](){
        failureActive = true;
      });
  }
}

Client::~Client()
{
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
    bclient[group]->Phase1(client_seq_num, txn, req->txnDigest, std::bind(
          &Client::Phase1Callback, this, req->id, group, std::placeholders::_1,
          std::placeholders::_2, std::placeholders::_3, std::placeholders::_4),
        std::bind(&Client::Phase1TimeoutCallback, this, group, req->id,
          std::placeholders::_1), req->timeout);
    req->outstandingPhase1s++;
  }
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

  req->outstandingPhase1s = 0;
  if (params.validateProofs && params.signedMessages) {
    req->slowAbortGroup = -1;
    req->p1ReplySigsGrouped.mutable_grouped_sigs()->clear();
    req->fast = true;
    req->decision = proto::COMMIT;
  }
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
  int64_t logGroup = GetLogGroup(txn, req->txnDigest);

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

  if (failureActive && params.injectFailure.type == InjectFailureType::CLIENT_CRASH &&
      txn.deps_size() > 0) {
    stats.Increment("inject_failure_crash");
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

} // namespace indicusstore
