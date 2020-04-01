// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * store/indicusstore/shardclient.cc:
 *   Single shard indicus transactional client.
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

#include "store/indicusstore/shardclient.h"

#include "store/indicusstore/common.h"

namespace indicusstore {

ShardClient::ShardClient(transport::Configuration *config, Transport *transport,
    uint64_t client_id, int shard, int closestReplica,
    bool signedMessages, bool validateProofs,
    KeyManager *keyManager, TrueTime &timeServer) :
    client_id(client_id), transport(transport), config(config), shard(shard),
    timeServer(timeServer),
    signedMessages(signedMessages), validateProofs(validateProofs),
    keyManager(keyManager), phase1DecisionTimeout(1000UL), lastReqId(0UL) {
  transport->Register(this, *config, -1, -1);

  if (closestReplica == -1) {
    replica = client_id % config->n;
  } else {
    replica = closestReplica;
  }
}

ShardClient::~ShardClient() {
}

void ShardClient::ReceiveMessage(const TransportAddress &remote,
      const std::string &t, const std::string &d, void *meta_data) {
  proto::SignedMessage signedMessage;
  proto::PackedMessage packedMessage;
  proto::ReadReply readReply;
  proto::Phase1Reply phase1Reply;
  proto::Phase2Reply phase2Reply;
  
  std::string type;
  std::string data;
  if (t == signedMessage.GetTypeName()) {
    if (!signedMessage.ParseFromString(d)) {
      return;
    }

    if (!ValidateSignedMessage(signedMessage, keyManager, data, type)) {
      return;
    }
  } else {
    type = t;
    data = d;
  }

  if (type == readReply.GetTypeName()) {
    readReply.ParseFromString(data);
    HandleReadReply(readReply);
  } else if (type == phase1Reply.GetTypeName()) {
    phase1Reply.ParseFromString(data);
    HandlePhase1Reply(phase1Reply, signedMessage);
  } else if (type == phase2Reply.GetTypeName()) {
    phase2Reply.ParseFromString(data);
    HandlePhase2Reply(phase2Reply, signedMessage);
  } else {
    Panic("Received unexpected message type: %s", type.c_str());
  }
}

void ShardClient::Begin(uint64_t id) {
  Debug("[shard %i] BEGIN: %lu", shard, id);

  txn = proto::Transaction();
}

void ShardClient::Get(uint64_t id, const std::string &key,
    const TimestampMessage &ts, uint64_t rqs, read_callback gcb,
    read_timeout_callback gtcb, uint32_t timeout) {
  if (BufferGet(key, gcb)) {
    return;
  }

  uint64_t reqId = lastReqId++;
  PendingQuorumGet *pendingGet = new PendingQuorumGet(reqId);
  pendingGets[reqId] = pendingGet;
  pendingGet->key = key;
  pendingGet->rqs = rqs;
  pendingGet->gcb = gcb;
  pendingGet->gtcb = gtcb;

  proto::Read read;
  read.set_req_id(reqId);
  read.set_key(key);
  *read.mutable_timestamp() = ts;

  transport->SendMessageToAll(this, read);

  Debug("[shard %i] Sent GET [%lu : %s]", shard, id, key.c_str());
}

void ShardClient::Put(uint64_t id, const std::string &key,
      const std::string &value, put_callback pcb, put_timeout_callback ptcb,
      uint32_t timeout) {
  WriteMessage *writeMsg = txn.add_write_set();
  writeMsg->set_key(key);
  writeMsg->set_value(value);
  pcb(REPLY_OK, key, value);
}

void ShardClient::Phase1(uint64_t id, const proto::Transaction &transaction,
    phase1_callback pcb, phase1_timeout_callback ptcb, uint32_t timeout) {
  Debug("[shard %i] Sending PHASE1 [%lu]", shard, id);
  uint64_t reqId = lastReqId++;
  PendingPhase1 *pendingPhase1 = new PendingPhase1(reqId);
  pendingPhase1s[reqId] = pendingPhase1;
  pendingPhase1->pcb = pcb;
  pendingPhase1->ptcb = ptcb;
  pendingPhase1->requestTimeout = new Timeout(transport, timeout, [this, pendingPhase1]() {
      phase1_timeout_callback ptcb = pendingPhase1->ptcb;
      auto itr = this->pendingPhase1s.find(pendingPhase1->reqId);
      if (itr != this->pendingPhase1s.end()) {
        this->pendingPhase1s.erase(itr);
        delete itr->second;
      }
      ptcb(REPLY_TIMEOUT);
  });

  // create prepare request
  proto::Phase1 phase1;
  phase1.set_req_id(reqId);
  phase1.set_txn_digest(TransactionDigest(transaction));
  *phase1.mutable_txn() = transaction;

  transport->SendMessageToAll(this, phase1);

  pendingPhase1->requestTimeout->Reset();
}

void ShardClient::Phase2(uint64_t id, const proto::Transaction &transaction,
    const std::map<int, std::vector<proto::Phase1Reply>> &groupedPhase1Replies,
    const std::map<int, std::vector<proto::SignedMessage>> &groupedSignedPhase1Replies,
    proto::CommitDecision decision, phase2_callback pcb,
    phase2_timeout_callback ptcb, uint32_t timeout) {
  Debug("[group %i] Sending PHASE1 [%lu]", shard, id);
  uint64_t reqId = lastReqId++;
  PendingPhase2 *pendingPhase2 = new PendingPhase2(reqId, decision);
  pendingPhase2s[reqId] = pendingPhase2;
  pendingPhase2->pcb = pcb;
  pendingPhase2->ptcb = ptcb;
  pendingPhase2->requestTimeout = new Timeout(transport, timeout, [this, pendingPhase2]() {
      phase2_timeout_callback ptcb = pendingPhase2->ptcb;
      auto itr = this->pendingPhase2s.find(pendingPhase2->reqId);
      if (itr != this->pendingPhase2s.end()) {
        this->pendingPhase2s.erase(itr);
        delete itr->second;
      }

      ptcb(REPLY_TIMEOUT);
  });

  // create prepare request
  proto::Phase2 phase2;
  phase2.set_req_id(reqId);
  phase2.set_txn_digest(TransactionDigest(transaction));
  if (validateProofs) {
    if (signedMessages) {
      for (const auto &group : groupedSignedPhase1Replies) {
        for (const auto &reply : group.second) {
          *(*phase2.mutable_signed_p1_replies()->mutable_replies())[group.first].add_msgs() = reply;
        }

      }
    } else {
      for (const auto &group : groupedPhase1Replies) {
        for (const auto &reply : group.second) {
          *(*phase2.mutable_p1_replies()->mutable_replies())[group.first].add_replies() = reply;
        }
      }
    }
  } else {
    phase2.set_decision(decision);
  }

  transport->SendMessageToAll(this, phase2);

  pendingPhase2->requestTimeout->Reset();
}

void ShardClient::Writeback(uint64_t id, const proto::Transaction &transaction,
    proto::CommitDecision decision, const proto::CommittedProof &proof,
    writeback_callback wcb, writeback_timeout_callback wtcb, uint32_t timeout) {
  uint64_t reqId = lastReqId++;
  PendingCommit *pendingCommit = new PendingCommit(reqId);
  pendingCommits[reqId] = pendingCommit;
  pendingCommit->ccb = wcb;
  pendingCommit->ctcb = wtcb;
  pendingCommit->requestTimeout = new Timeout(transport, timeout, [this, reqId]() {
      auto itr = this->pendingCommits.find(reqId);
      if (itr != this->pendingCommits.end()) {
        commit_timeout_callback ctcb = itr->second->ctcb;
        this->pendingCommits.erase(itr);
        delete itr->second;
        ctcb(REPLY_TIMEOUT);
      }

  });

  // create commit request
  proto::Writeback writeback;
  writeback.set_req_id(reqId);
  writeback.set_txn_digest(TransactionDigest(transaction));
  *writeback.mutable_txn() = transaction;
  writeback.set_decision(decision);

  transport->SendMessageToAll(this, writeback);
  Debug("[shard %i] Sent WRITEBACK [%lu]", shard, id);

  pendingCommit->requestTimeout->Reset();
}

bool ShardClient::BufferGet(const std::string &key, read_callback rcb) {
  for (const auto &write : txn.write_set()) {
    if (write.key() == key) {
      rcb(REPLY_OK, key, write.value(), Timestamp(), proto::Transaction(),
          false);
      return true;
    }
  }

  /* TODO: cache value already read by txn
  for (const auto &read : txn.read_set()) {
    if (read.key() == key) {
      
    }
  }*/

  return false;
}

void ShardClient::GetTimeout(uint64_t reqId) {
  auto itr = this->pendingGets.find(reqId);
  if (itr != this->pendingGets.end()) {
    get_timeout_callback gtcb = itr->second->gtcb;
    std::string key = itr->second->key;
    this->pendingGets.erase(itr);
    delete itr->second;
    gtcb(REPLY_TIMEOUT, key);
  }
}

/* Callback from a shard replica on get operation completion. */
void ShardClient::HandleReadReply(const proto::ReadReply &reply) {
  auto itr = this->pendingGets.find(reply.req_id());
  if (itr == this->pendingGets.end()) {
    return; // this is a stale request
  }

  if (validateProofs) {
    if (!ValidateTransactionWrite(reply.committed_proof(), itr->second->key,
          reply.committed_value(), reply.committed_timestamp(), config,
          signedMessages, keyManager)) {
      // invalid replies can be treated as if we never received a reply from
      //     a crashed replica
      return;
    }
  }

  // value and timestamp are valid
  itr->second->numReplies++;
  if (reply.status() == REPLY_OK) {
    itr->second->numOKReplies++;
    Timestamp replyTs(reply.committed_timestamp());
    if (itr->second->maxTs < replyTs) {
      itr->second->maxTs = replyTs;
      itr->second->maxValue = reply.committed_value();
    }
  }

  proto::PreparedWrite prepared;
  bool hasPrepared = false;
  if (signedMessages) {
    if (reply.has_signed_prepared()) {
      if (ValidateSignedMessage(reply.signed_prepared(), keyManager, prepared)) {
        hasPrepared = true;
      } else {
        // TODO: should we continue with the committed value?
        return;
      }
    }
  } else {
    if (reply.has_prepared()) {
      hasPrepared = true;
      prepared = reply.prepared();
    }
  }

  if (hasPrepared) {
    Timestamp preparedTs(prepared.timestamp());
    if (itr->second->maxTs < preparedTs) {
      itr->second->maxTs = preparedTs;
      itr->second->maxValue = prepared.value();
      itr->second->dep = prepared.txn();
    }
  }

  if (itr->second->numOKReplies >= itr->second->rqs ||
      itr->second->numReplies == static_cast<uint64_t>(config->n)) {
    PendingQuorumGet *req = itr->second;
    read_callback gcb = req->gcb;
    std::string key = req->key;
    pendingGets.erase(itr);
    int32_t status = REPLY_FAIL;
    if (req->numOKReplies >= itr->second->rqs) {
      status = REPLY_OK;
    }
    Debug("[shard %lu:%i] GET callback [%d]", client_id, shard, status);
    gcb(status, key, req->maxValue, req->maxTs, req->dep, req->hasDep);
    delete req;
  }
}

void ShardClient::HandlePhase1Reply(const proto::Phase1Reply &reply,
    const proto::SignedMessage &signedReply) {
  auto itr = this->pendingPhase1s.find(reply.req_id());
  if (itr == this->pendingPhase1s.end()) {
    return; // this is a stale request
  }

  Debug("[shard %lu:%i] PHASE1 callback [%d] ccr=%d", client_id, shard,
      reply.status(), reply.ccr());

  if (signedMessages) {
    itr->second->signedPhase1Replies.push_back(signedReply);
  }
  itr->second->phase1Replies.push_back(reply);

  if (itr->second->phase1Replies.size() == static_cast<size_t>(config->n)) {
    Phase1Decision(itr);
  } else if (itr->second->phase1Replies.size() >= QuorumSize() &&
      !itr->second->decisionTimeoutStarted) {
    uint64_t reqId = reply.req_id();
    itr->second->decisionTimeout = new Timeout(transport,
        phase1DecisionTimeout, [this, reqId]() {
          Phase1Decision(reqId);      
        }
      );
    itr->second->decisionTimeout->Reset();
    itr->second->decisionTimeoutStarted = true;
  }
}

void ShardClient::HandlePhase2Reply(const proto::Phase2Reply &reply,
    const proto::SignedMessage &signedReply) {
  auto itr = this->pendingPhase2s.find(reply.req_id());
  if (itr == this->pendingPhase2s.end()) {
    return; // this is a stale request
  }

  if (validateProofs) {
    itr->second->phase2Replies.push_back(reply);
    if (signedMessages) {
      itr->second->signedPhase2Replies.push_back(signedReply);
    }
  }

  if (reply.decision() == itr->second->decision) {
    itr->second->matchingReplies++;
  }

  if (itr->second->matchingReplies >= 4 * static_cast<uint64_t>(config->f) + 1) {
    phase2_callback pcb = itr->second->pcb;
    pcb(itr->second->phase2Replies, itr->second->signedPhase2Replies);
    this->pendingPhase2s.erase(itr);
    delete itr->second;
  }
}

/* Callback from a shard replica on commit operation completion. */
bool ShardClient::CommitCallback(uint64_t reqId,
    const string &request_str, const string &reply_str) {
  // COMMITs always succeed.
  Debug("[shard %lu:%i] COMMIT callback", client_id, shard);

  auto itr = this->pendingCommits.find(reqId);
  if (itr != this->pendingCommits.end()) {
    writeback_callback ccb = itr->second->ccb;
    this->pendingCommits.erase(itr);
    delete itr->second;
    ccb();
  }
  return true;
}

/* Callback from a shard replica on abort operation completion. */
bool ShardClient::AbortCallback(uint64_t reqId,
    const string &request_str, const string &reply_str) {
  // ABORTs always succeed.

  // UW_ASSERT(blockingBegin != NULL);
  // blockingBegin->Reply(0);

  Debug("[shard %lu:%i] ABORT callback", client_id, shard);

  auto itr = this->pendingAborts.find(reqId);
  if (itr != this->pendingAborts.end()) {
    abort_callback acb = itr->second->acb;
    this->pendingAborts.erase(itr);
    delete itr->second;
    acb();
  }
  return true;
}

void ShardClient::Phase1Decision(uint64_t reqId) {
  auto itr = this->pendingPhase1s.find(reqId);
  if (itr == this->pendingPhase1s.end()) {
    return; // this is a stale request
  }

  Phase1Decision(itr);
}

void ShardClient::Phase1Decision(
    std::unordered_map<uint64_t, PendingPhase1 *>::iterator itr) {
  bool fast = false;
  proto::CommitDecision decision = IndicusShardDecide(itr->second->phase1Replies,
      config, validateProofs, signedMessages, keyManager, fast);
  phase1_callback pcb = itr->second->pcb;
  pcb(decision, fast, itr->second->phase1Replies,
      itr->second->signedPhase1Replies);
  this->pendingPhase1s.erase(itr);
  delete itr->second;
}

} // namespace indicus
