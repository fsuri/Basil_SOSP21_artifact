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
    uint64_t readQuorumSize, TrueTime &timeServer) : client_id(client_id),
    transport(transport), config(config), shard(shard),
    readQuorumSize(readQuorumSize), timeServer(timeServer),
    signedMessages(false), validateProofs(false), cryptoConfig(nullptr),
    lastReqId(0UL) {
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
  proto::ReadReply readReply;
  proto::Phase1Reply phase1Reply;
  proto::Phase2Reply phase2Reply;
  
  std::string type;
  std::string data;
  if (t == signedMessage.GetTypeName()) {
    signedMessage.ParseFromString(d);
    if (ValidateSignedMessage(signedMessage, cryptoConfig)) {
      type = signedMessage.type();
      data = signedMessage.msg();
    } else {
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
    HandlePhase2Reply(phase2Reply);
  } else {
    Panic("Received unexpected message type: %s", type.c_str());
  }
}

void ShardClient::Begin(uint64_t id) {
  Debug("[shard %i] BEGIN: %lu", shard, id);

  txn = proto::Transaction();
  txn.set_id(id);
}

void ShardClient::Get(uint64_t id, const std::string &key, read_callback gcb,
      read_timeout_callback gtcb, uint32_t timeout) {
  if (BufferGet(key, gcb)) {
    return;
  }

  uint64_t reqId = lastReqId++;
  PendingQuorumGet *pendingGet = new PendingQuorumGet(reqId);
  pendingGets[reqId] = pendingGet;
  pendingGet->key = key;
  pendingGet->gcb = gcb;
  pendingGet->gtcb = gtcb;

  proto::Read read;
  read.set_req_id(reqId);
  read.set_key(key);
  Timestamp localTs(timeServer.GetTime());
  localTs.serialize(read.mutable_timestamp());

  transport->SendMessageToAll(this, read);

  Debug("[shard %i] Sent GET [%lu : %s]", shard, id, key.c_str());
}

void ShardClient::Put(uint64_t id, const std::string &key,
      const std::string &value, put_callback pcb, put_timeout_callback ptcb,
      uint32_t timeout) {
  WriteMessage *writeMsg = txn.add_writeset();
  writeMsg->set_key(key);
  writeMsg->set_value(value);
  pcb(REPLY_OK, key, value);
}

void ShardClient::Phase1(uint64_t id, const Timestamp &timestamp,
    phase1_callback pcb, phase1_timeout_callback ptcb, uint32_t timeout) {
  Debug("[shard %i] Sending PREPARE [%lu]", shard, id);
  uint64_t reqId = lastReqId++;
  PendingPhase1 *pendingPhase1 = new PendingPhase1(reqId);
  pendingPhase1s[reqId] = pendingPhase1;
  pendingPhase1->ts = timestamp;
  pendingPhase1->txn = txn;
  pendingPhase1->pcb = pcb;
  pendingPhase1->ptcb = ptcb;
  pendingPhase1->requestTimeout = new Timeout(transport, timeout, [this, pendingPhase1]() {
      Timestamp ts = pendingPhase1->ts;
      phase1_timeout_callback ptcb = pendingPhase1->ptcb;
      auto itr = this->pendingPhase1s.find(pendingPhase1->reqId);
      if (itr != this->pendingPhase1s.end()) {
        this->pendingPhase1s.erase(itr);
        delete itr->second;
      }

      ptcb(REPLY_TIMEOUT, ts);
  });

  timestamp.serialize(txn.mutable_timestamp());

  // create prepare request
  proto::Phase1 phase1;
  phase1.set_req_id(reqId);
  phase1.set_txn_id(id);
  *phase1.mutable_txn() = txn;

  transport->SendMessageToAll(this, phase1);

  pendingPhase1->requestTimeout->Reset();
}

void ShardClient::Commit(uint64_t id, uint64_t timestamp, commit_callback ccb,
    commit_timeout_callback ctcb, uint32_t timeout) {
  uint64_t reqId = lastReqId++;
  PendingCommit *pendingCommit = new PendingCommit(reqId);
  pendingCommits[reqId] = pendingCommit;
  pendingCommit->ts = timestamp;
  pendingCommit->txn = txn;
  pendingCommit->ccb = ccb;
  pendingCommit->ctcb = ctcb;
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

  transport->SendMessageToAll(this, writeback);
  Debug("[shard %i] Sent WRITEBACK [%lu]", shard, id);

  pendingCommit->requestTimeout->Reset();
}

void ShardClient::Abort(uint64_t id, abort_callback acb,
    abort_timeout_callback atcb, uint32_t timeout) {
  Debug("[shard %i] Sending ABORT [%lu]", shard, id);

  uint64_t reqId = lastReqId++;
  PendingAbort *pendingAbort = new PendingAbort(reqId);
  pendingAborts[reqId] = pendingAbort;
  pendingAbort->txn = txn;
  pendingAbort->acb = acb;
  pendingAbort->atcb = atcb;
  pendingAbort->requestTimeout = new Timeout(transport, timeout, [this, reqId]() {
      auto itr = this->pendingAborts.find(reqId);
      if (itr != this->pendingAborts.end()) {
        abort_timeout_callback atcb = itr->second->atcb;
        this->pendingAborts.erase(itr);
        delete itr->second;
        atcb(REPLY_TIMEOUT);
      }
  });

  // create abort request
  proto::Abort abort;
  abort.set_req_id(reqId);

  transport->SendMessageToAll(this, abort);

  pendingAbort->requestTimeout->Reset();
}

bool ShardClient::BufferGet(const std::string &key, read_callback rcb) {
  for (const auto &write : txn.writeset()) {
    if (write.key() == key) {
      rcb(REPLY_OK, key, write.value(), Timestamp(), proto::Transaction(),
          false);
      return true;
    }
  }

  /* TODO: cache value already read by txn
  for (const auto &read : txn.readset()) {
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
    // TODO: does write proof need to be signed?
    if (!ValidateWriteProof(reply.committed_proof(), itr->second->key,
          reply.committed_value(), reply.committed_timestamp())) {
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
      if (ValidateSignedMessage(reply.signed_prepared(), cryptoConfig)) {
        prepared.ParseFromString(reply.signed_prepared().msg());
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

  if (itr->second->numOKReplies >= readQuorumSize ||
      itr->second->numReplies == static_cast<uint64_t>(config->n)) {
    PendingQuorumGet *req = itr->second;
    read_callback gcb = req->gcb;
    std::string key = req->key;
    pendingGets.erase(itr);
    int32_t status = REPLY_FAIL;
    if (req->numOKReplies >= readQuorumSize) {
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

  Debug("[shard %lu:%i] PHASE1 callback [%d]", client_id, shard,
      reply.status());

  if (signedMessages) {
    itr->second->signedPhase1Replies.push_back(signedReply);
  }
  itr->second->phase1Replies.push_back(reply);

  if (itr->second->phase1Replies.size() == static_cast<size_t>(config->n)) {
    Timestamp retryTs;
    proto::CommitDecision decision = IndicusDecide(itr->second->phase1Replies,
        config);
    phase1_callback pcb = itr->second->pcb;
    this->pendingPhase1s.erase(itr);
    delete itr->second;
    if (reply.has_retry_timestamp()) {
      pcb(decision, Timestamp(reply.retry_timestamp()));
    } else {
      pcb(decision, Timestamp());
    }
  }
}
void ShardClient::HandlePhase2Reply(const proto::Phase2Reply &phase2Reply) {
}

/* Callback from a shard replica on commit operation completion. */
bool ShardClient::CommitCallback(uint64_t reqId,
    const string &request_str, const string &reply_str) {
  // COMMITs always succeed.
  Debug("[shard %lu:%i] COMMIT callback", client_id, shard);

  auto itr = this->pendingCommits.find(reqId);
  if (itr != this->pendingCommits.end()) {
    commit_callback ccb = itr->second->ccb;
    this->pendingCommits.erase(itr);
    delete itr->second;
    ccb(true);
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

bool ShardClient::ValidateWriteProof(const proto::WriteProof &proof,
    const std::string &key, const std::string &val, const Timestamp &timestamp) {
  return true;
}

} // namespace indicus
