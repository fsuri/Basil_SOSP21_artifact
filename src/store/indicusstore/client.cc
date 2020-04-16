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

namespace indicusstore {

using namespace std;

Client::Client(transport::Configuration *config, uint64_t id, int nShards,
    int nGroups,
    const std::vector<int> &closestReplicas, Transport *transport,
    partitioner part, bool syncCommit,
    uint64_t readQuorumSize, bool signedMessages, bool validateProofs,
    KeyManager *keyManager, TrueTime timeServer) : config(config),
    client_id(id),
    nshards(nShards), ngroups(nGroups), transport(transport), part(part),
    syncCommit(syncCommit), readQuorumSize(readQuorumSize),
    signedMessages(signedMessages), validateProofs(validateProofs),
    keyManager(keyManager), timeServer(timeServer), client_seq_num(0UL),
    lastReqId(0UL) {
  bclient.reserve(nshards);

  Debug("Initializing Indicus client with id [%lu] %lu", client_id, nshards);

  /* Start a client for each shard. */
  for (uint64_t i = 0; i < ngroups; i++) {
    bclient[i] = new ShardClient(config, transport, client_id, i,
        closestReplicas, signedMessages, validateProofs,
        keyManager, timeServer);
  }

  Debug("Indicus client [%lu] created! %lu %lu", client_id, nshards,
      bclient.size());
}

Client::~Client()
{
    for (auto b : bclient) {
        delete b;
    }
}

/* Begins a transaction. All subsequent operations before a commit() or
 * abort() are part of this transaction.
 */
void Client::Begin() {
  client_seq_num++;
  Debug("BEGIN [%lu]", client_seq_num);

  txn = proto::Transaction();
  txn.set_client_id(client_id);
  txn.set_client_seq_num(client_seq_num);
  // Optimistically choose a read timestamp for all reads in this transaction
  txn.mutable_timestamp()->set_timestamp(timeServer.GetTime());
  txn.mutable_timestamp()->set_id(client_id);
}

void Client::Get(const std::string &key, get_callback gcb,
    get_timeout_callback gtcb, uint32_t timeout) {
  Debug("GET [%lu : %s]", client_seq_num, key.c_str());

  // Contact the appropriate shard to get the value.
  int i = part(key, nshards) % ngroups;

  // If needed, add this shard to set of participants and send BEGIN.
  if (!IsParticipant(i)) {
    txn.add_involved_groups(i);
    bclient[i]->Begin(client_seq_num);
  }

  read_callback rcb = [gcb, this](int status, const std::string &key,
      const std::string &val, const Timestamp &ts, const proto::Dependency &dep,
      bool hasDep, bool addReadSet) {
    if (Message_DebugEnabled(__FILE__)) {
      Debug("GET[%lu] Callback for key %s with %lu bytes and ts %lu.%lu.",
          client_seq_num, BytesToHex(key, 16).c_str(), val.length(),
          ts.getTimestamp(), ts.getID());
      if (hasDep) {
        Debug("GET[%lu] Callback for key %s with dep ts %lu.%lu.",
            client_seq_num, BytesToHex(key, 16).c_str(),
            dep.prepared().timestamp().timestamp(),
            dep.prepared().timestamp().id());
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
  bclient[i]->Get(client_seq_num, key, txn.timestamp(), readQuorumSize, rcb,
      rtcb, timeout);
}

void Client::Put(const std::string &key, const std::string &value,
    put_callback pcb, put_timeout_callback ptcb, uint32_t timeout) {
  if (Message_DebugEnabled(__FILE__)) {
    uint64_t intValue = 0;
    for (int i = 0; i < 4; ++i) {
      intValue = intValue | (static_cast<uint64_t>(value[i]) << ((3 - i) * 8));
    }
    Debug("PUT [%lu : %s : %lu]", client_seq_num, key.c_str(), intValue);
  }

  // Contact the appropriate shard to set the value.
  int i = part(key, nshards) % ngroups;

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
}

void Client::Commit(commit_callback ccb, commit_timeout_callback ctcb,
    uint32_t timeout) {
  PendingRequest *req = new PendingRequest(client_seq_num);
  pendingReqs[client_seq_num] = req;
  req->ccb = ccb;
  req->ctcb = ctcb;
  req->callbackInvoked = false;
  
  Phase1(req, timeout);
}

void Client::Phase1(PendingRequest *req, uint32_t timeout) {
  Debug("PHASE1 [%lu:%lu] at %lu", client_id, client_seq_num,
      txn.timestamp().timestamp());
  UW_ASSERT(txn.involved_groups().size() > 0);

  for (auto group : txn.involved_groups()) {
    bclient[group]->Phase1(client_seq_num, txn, std::bind(
          &Client::Phase1Callback, this, req->id, group, std::placeholders::_1,
          std::placeholders::_2, std::placeholders::_3, std::placeholders::_4),
        std::bind(&Client::Phase1TimeoutCallback, this, req->id,
          std::placeholders::_1), timeout);
    req->outstandingPhase1s++;
  }
}

void Client::Phase1Callback(uint64_t txnId, int group,
    proto::CommitDecision decision, bool fast,
    const std::vector<proto::Phase1Reply> &phase1Replies,
    const std::vector<proto::SignedMessage> &signedPhase1Replies) {
  Debug("PHASE1[%lu] callback decision %d from group %d", client_seq_num,
      decision, group);
  auto itr = this->pendingReqs.find(txnId);
  if (itr == this->pendingReqs.end()) {
    Debug("Phase1Callback for terminated request %lu (txn already committed"
        " or aborted.", txnId);
    return;
  }
  PendingRequest *req = itr->second;

  if (req->startedPhase2) {
    Debug("Already started Phase2 for request id %lu. Ignoring Phase1 response "
        "from group %d.", txnId, group);
    return;
  }

  if (validateProofs) {
    req->phase1RepliesGrouped[group] = phase1Replies;
    if (signedMessages) {
      req->signedPhase1RepliesGrouped[group] = signedPhase1Replies;
    }
  }

  req->fast = req->fast && fast;
  --req->outstandingPhase1s;
  switch(decision) {
    case proto::COMMIT:
      Debug("PHASE1[%lu] COMMIT %d", client_seq_num, group);
      break;
    case proto::ABORT:
      // abort!
      Debug("PHASE1[%lu] ABORT %d", client_seq_num, group);
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

void Client::Phase1TimeoutCallback(uint64_t txnId, int status) {
}

void Client::HandleAllPhase1Received(PendingRequest *req) {
  Debug("All PHASE1's [%lu] received", client_seq_num);
  if (req->fast) {
    Writeback(req, 3000);
  } else {
    // slow path, must log final result to 1 group
    Phase2(req, 3000); // todo: do we care about timeouts?
  }
}

void Client::Phase2(PendingRequest *req, uint32_t timeout) {
  req->txnDigest = TransactionDigest(txn);

  Debug("PHASE2[%lu][%s]", client_seq_num,
      BytesToHex(req->txnDigest, 16).c_str());

  int logGroup = TransactionDigest(txn)[0] % ngroups;
  req->startedPhase2 = true;
  bclient[logGroup]->Phase2(client_seq_num, req->txnDigest,
      req->phase1RepliesGrouped,
      req->signedPhase1RepliesGrouped, req->decision,
      std::bind(&Client::Phase2Callback, this, req->id,
        std::placeholders::_1, std::placeholders::_2),
      std::bind(&Client::Phase2TimeoutCallback, this, req->id,
        std::placeholders::_1), timeout);
}

void Client::Phase2Callback(uint64_t txnId,
    const std::vector<proto::Phase2Reply> &phase2Replies,
    const std::vector<proto::SignedMessage> &signedPhase2Replies) {
  Debug("PHASE2[%lu] callback", txnId);

  auto itr = this->pendingReqs.find(txnId);
  if (itr == this->pendingReqs.end()) {
    Debug("Phase2Callback for terminated request id %lu (txn already committed or aborted.", txnId);
    return;
  }

  if (validateProofs) {
    itr->second->phase2Replies = phase2Replies;
    if (signedMessages) {
      itr->second->signedPhase2Replies = signedPhase2Replies;
    }
  }

  Writeback(itr->second, 3000);
}

void Client::Phase2TimeoutCallback(uint64_t txnId, int status) {
}

void Client::Writeback(PendingRequest *req, uint32_t timeout) {
  Debug("WRITEBACK[%lu]", req->id);

  int result;
  switch (req->decision) {
    case proto::COMMIT: {
      Debug("COMMIT[%lu]", req->id);
      result = RESULT_COMMITTED;
      break;
    }
    case proto::ABORT: {
      result = RESULT_SYSTEM_ABORTED;
      req->txnDigest = TransactionDigest(txn);
      break;
    }
    default: {
      break;
    }
  }

  writeback_callback wcb = [this, req, result]() {
    auto itr = this->pendingReqs.find(req->id);
    if (itr != this->pendingReqs.end()) {
      if (!itr->second->callbackInvoked) {
        itr->second->ccb(result);
        itr->second->callbackInvoked = true;
      }
      this->pendingReqs.erase(itr);
      delete req;
    }
  };

  proto::CommittedProof proof;
  if (validateProofs) {
    if (req->fast) {
      *proof.mutable_txn() = txn;
      if (signedMessages) {
        for (const auto &signedPhase1Reply : req->signedPhase1RepliesGrouped) {
          for (const auto &signedReply : signedPhase1Reply.second) {
            *(*proof.mutable_signed_p1_replies()->mutable_replies())[signedPhase1Reply.first].add_msgs() = signedReply;
          }
        }
      } else {
        for (const auto &phase1Reply : req->phase1RepliesGrouped) {
          for (const auto &reply : phase1Reply.second) {
            *(*proof.mutable_p1_replies()->mutable_replies())[phase1Reply.first].add_replies() = reply;
          }
        }
      }
    } else {
      if (signedMessages) {
        for (const auto &signedPhase2Reply : req->signedPhase2Replies) {
          *proof.mutable_signed_p2_replies()->add_msgs() = signedPhase2Reply;
        }

      } else {
        proto::Phase2Replies p2Replies;
        for (const auto &phase2Reply : req->phase2Replies) {
          *proof.mutable_p2_replies()->add_replies() = phase2Reply;
        }
      }
    }
  }

  // TODO: what to do when Writeback times out
  writeback_timeout_callback wtcb = [](int status){};
  for (auto group : txn.involved_groups()) {
    bclient[group]->Writeback(client_seq_num, txn, req->txnDigest,
        req->decision, proof, wcb, wtcb, timeout);
  }

  if (!req->callbackInvoked) {
    req->ccb(result);
    req->callbackInvoked = true;
  }
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
  // presumably this will be called with empty callbacks as the application can
  // immediately move on to its next transaction without waiting for confirmation
  // that this transaction was aborted
  Debug("ABORT[%lu]", client_seq_num);


  proto::CommittedProof proof;
  writeback_callback wcb = []() {};
  
  std::string txnDigest = TransactionDigest(txn);
  writeback_timeout_callback wtcb = [](int status){};
  for (auto group : txn.involved_groups()) {
    bclient[group]->Writeback(client_seq_num, txn, txnDigest,
        proto::ABORT, proof, wcb, wtcb, timeout);
  }
  // TODO: can we just call callback immediately?
  acb();
}

/* Return statistics of most recent transaction. */
vector<int>
Client::Stats()
{
    vector<int> v;
    return v;
}

} // namespace indicusstore
