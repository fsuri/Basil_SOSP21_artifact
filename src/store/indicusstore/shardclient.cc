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


#include "store/benchmark/async/tpcc/tpcc-proto.pb.h"

namespace indicusstore {

using namespace std;
using namespace proto;

ShardClient::ShardClient(transport::Configuration *config, Transport *transport,
    uint64_t client_id, int shard, int closestReplica,
    uint64_t readQuorumSize) : client_id(client_id),
    transport(transport), config(config), shard(shard),
    readQuorumSize(readQuorumSize) {
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
      const std::string &type, const std::string &data, void *meta_data) {
  proto::ReadReply readReply;
  proto::PrepareReply prepareReply;

  if (type == readReply.GetTypeName()) {
    readReply.ParseFromString(data);
    HandleReadReply(readReply);
  } else if (type == prepareReply.GetTypeName()) {
    prepareReply.ParseFromString(data);
    HandlePrepareReply(prepareReply);
  } else {
    Panic("Received unexpected message type: %s", type.c_str());
  }
}



void ShardClient::Begin(uint64_t id) {
  Debug("[shard %i] BEGIN: %lu", shard, id);
}

void ShardClient::Get(uint64_t id, const std::string &key, get_callback gcb,
      get_timeout_callback gtcb, uint32_t timeout) {
  // Send the GET operation to appropriate shard.

  uint64_t reqId = lastReqId++;
  PendingQuorumGet *pendingGet = new PendingQuorumGet(reqId);
  pendingGets[reqId] = pendingGet;
  pendingGet->key = key;
  pendingGet->gcb = gcb;
  pendingGet->gtcb = gtcb;

  proto::Read read;
  read.set_req_id(reqId);
  read.set_key(key);

  transport->SendMessageToAll(this, read);

  Debug("[shard %i] Sent GET [%lu : %s]", shard, id, key.c_str());
}

void ShardClient::Get(uint64_t id, const std::string &key,
    const Timestamp &timestamp, get_callback gcb, get_timeout_callback gtcb,
    uint32_t timeout) {
  Get(id, key, gcb, gtcb, timeout);
}

void ShardClient::Put(uint64_t id, const std::string &key,
      const std::string &value, put_callback pcb, put_timeout_callback ptcb,
      uint32_t timeout) {
  Panic("Unimplemented PUT");
  return;
}

void ShardClient::Prepare(uint64_t id, const Transaction &txn,
      const Timestamp &timestamp, prepare_callback pcb,
      prepare_timeout_callback ptcb, uint32_t timeout) {
  Debug("[shard %i] Sending PREPARE [%lu]", shard, id);

  uint64_t reqId = lastReqId++;
  PendingPrepare *pendingPrepare = new PendingPrepare(reqId);
  pendingPrepares[reqId] = pendingPrepare;
  pendingPrepare->ts = timestamp;
  pendingPrepare->txn = txn;
  pendingPrepare->pcb = pcb;
  pendingPrepare->ptcb = ptcb;
  pendingPrepare->requestTimeout = new Timeout(transport, timeout, [this, pendingPrepare]() {
      Timestamp ts = pendingPrepare->ts;
      prepare_timeout_callback ptcb = pendingPrepare->ptcb;
      auto itr = this->pendingPrepares.find(pendingPrepare->reqId);
      if (itr != this->pendingPrepares.end()) {
        this->pendingPrepares.erase(itr);
        delete itr->second;
      }

      ptcb(REPLY_TIMEOUT, ts);
  });

  // create prepare request
  proto::Prepare prepare;
  prepare.set_req_id(reqId);
  txn.serialize(prepare.mutable_txn());
  timestamp.serialize(prepare.mutable_timestamp());

  transport->SendMessageToAll(this, prepare);

  pendingPrepare->requestTimeout->Reset();
}

void ShardClient::Commit(uint64_t id, const Transaction & txn,
      uint64_t timestamp, commit_callback ccb, commit_timeout_callback ctcb,
      uint32_t timeout) {
  
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
  proto::Commit commit;
  commit.set_req_id(reqId);
  commit.set_timestamp(timestamp);

  transport->SendMessageToAll(this, commit);
  Debug("[shard %i] Sent COMMIT [%lu]", shard, id);

  pendingCommit->requestTimeout->Reset();
}  
  
void ShardClient::Abort(uint64_t id, const Transaction &txn,
      abort_callback acb, abort_timeout_callback atcb, uint32_t timeout) {
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
  txn.serialize(abort.mutable_txn());

  transport->SendMessageToAll(this, abort);

  pendingAbort->requestTimeout->Reset();
}

std::string ShardClient::IndicusDecide(const std::map<std::string,std::size_t> &results) {
  // If a majority say prepare_ok,
  int ok_count = 0;
  Timestamp ts = 0;
  string final_reply_str;
  PrepareReply final_reply;

  for (const auto& string_and_count : results) {
    const std::string &s = string_and_count.first;
    const std::size_t count = string_and_count.second;

    PrepareReply reply;
    reply.ParseFromString(s);

    if (reply.status() == REPLY_OK) {
      ok_count += count;
    } else if (reply.status() == REPLY_FAIL) {
      return s;
    } else if (reply.status() == REPLY_RETRY) {
      Timestamp t(reply.timestamp());
      if (t > ts) {
        ts = t;
      }
    }
  }

  if (ok_count >= config->QuorumSize()) {
    final_reply.set_status(REPLY_OK);
  } else {
    final_reply.set_status(REPLY_RETRY);
    ts.serialize(final_reply.mutable_timestamp());
  }
  final_reply.SerializeToString(&final_reply_str);
  return final_reply_str;
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

  itr->second->numReplies++;
  if (reply.status() == REPLY_OK) {
    itr->second->numOKReplies++;
    Timestamp replyTs(reply.timestamp());
    if (itr->second->maxTs < replyTs) {
      itr->second->maxTs = replyTs;
      itr->second->maxValue = reply.value();
    }
  }

  if (itr->second->numOKReplies >= readQuorumSize ||
      itr->second->numReplies == static_cast<uint64_t>(config->n)) {
    PendingQuorumGet *req = itr->second;
    get_callback gcb = req->gcb;
    std::string key = req->key;
    pendingGets.erase(itr);
    int32_t status = REPLY_FAIL;
    if (req->numOKReplies >= readQuorumSize) {
      status = REPLY_OK;
    }
    Debug("[shard %lu:%i] GET callback [%d]", client_id, shard, status);
    gcb(status, key, req->maxValue, req->maxTs);
    delete req;
  }
}

/* Callback from a shard replica on prepare operation completion. */
void ShardClient::HandlePrepareReply(const proto::PrepareReply &reply) {
  auto itr = this->pendingPrepares.find(reply.req_id());
  if (itr == this->pendingPrepares.end()) {
    return; // this is a stale request
  }

  Debug("[shard %lu:%i] PREPARE callback [%d]", client_id, shard,
      reply.status());

  prepare_callback pcb = itr->second->pcb;
  this->pendingPrepares.erase(itr);
  delete itr->second;
  if (reply.has_timestamp()) {
    pcb(reply.status(), Timestamp(reply.timestamp()));
  } else {
    pcb(reply.status(), Timestamp());
  }
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

} // namespace indicus