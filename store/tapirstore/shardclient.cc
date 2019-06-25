// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * store/tapirstore/shardclient.cc:
 *   Single shard tapir transactional client.
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

#include "store/tapirstore/shardclient.h"


#include "store/benchmark/async/tpcc/tpcc-proto.pb.h"

namespace tapirstore {

using namespace std;
using namespace proto;

ShardClient::ShardClient(const string &configPath, Transport *transport,
    uint64_t client_id, int shard, int closestReplica) : client_id(client_id),
      transport(transport), shard(shard), lastReqId(0UL) {
  ifstream configStream(configPath);
  if (configStream.fail()) {
    Panic("Unable to read configuration file: %s\n", configPath.c_str());
  }

  transport::Configuration config(configStream);
  this->config = &config;

  client = new replication::ir::IRClient(config, transport, client_id);

  if (closestReplica == -1) {
    replica = client_id % config.n;
  } else {
    replica = closestReplica;
  }
  Debug("Sending unlogged to replica %i", replica);
}

ShardClient::~ShardClient() {
    delete client;
}

void ShardClient::Begin(uint64_t id) {
  Debug("[shard %i] BEGIN: %lu", shard, id);
}

void ShardClient::Get(uint64_t id, const std::string &key, get_callback gcb,
      get_timeout_callback gtcb, uint32_t timeout) {
  // Send the GET operation to appropriate shard.

  // create request
  string request_str;
  Request request;
  request.set_op(Request::GET);
  request.set_txnid(id);
  request.mutable_get()->set_key(key);
  request.SerializeToString(&request_str);

  uint64_t reqId = lastReqId++;
  PendingGet *pendingGet = new PendingGet(reqId);
  pendingGets[reqId] = pendingGet;
  pendingGet->key = key;
  pendingGet->gcb = gcb;
  pendingGet->gtcb = gtcb;

  client->InvokeUnlogged(replica, request_str, bind(&ShardClient::GetCallback,
      this, pendingGet->reqId, placeholders::_1, placeholders::_2),
      bind(&ShardClient::GetTimeout, this, pendingGet->reqId), timeout);

  Debug("[shard %i] Sent GET [%lu : %s]", shard, id, key.c_str());
}

void ShardClient::Get(uint64_t id, const std::string &key,
    const Timestamp &timestamp, get_callback gcb, get_timeout_callback gtcb,
    uint32_t timeout) {

  // Send the GET operation to appropriate shard.
  Debug("[shard %i] Sending GET [%lu : %s]", shard, id, key.c_str());

  // create request
  string request_str;
  Request request;
  request.set_op(Request::GET);
  request.set_txnid(id);
  request.mutable_get()->set_key(key);
  timestamp.serialize(request.mutable_get()->mutable_timestamp());
  request.SerializeToString(&request_str);

  uint64_t reqId = lastReqId++;
  PendingGet *pendingGet = new PendingGet(reqId);
  pendingGets[reqId] = pendingGet;
  pendingGet->key = key;
  pendingGet->gcb = gcb;
  pendingGet->gtcb = gtcb;

  client->InvokeUnlogged(replica, request_str, bind(&ShardClient::GetCallback,
      this, pendingGet->reqId, placeholders::_1, placeholders::_2),
      bind(&ShardClient::GetTimeout, this, pendingGet->reqId), timeout);
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

  // create prepare request
  string request_str;
  Request request;
  request.set_op(Request::PREPARE);
  request.set_txnid(id);
  txn.serialize(request.mutable_prepare()->mutable_txn());
  timestamp.serialize(request.mutable_prepare()->mutable_timestamp());
  request.SerializeToString(&request_str);

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

  client->InvokeConsensus(request_str, std::bind(&ShardClient::TapirDecide, this,
      placeholders::_1), std::bind(&ShardClient::PrepareCallback, this,
      pendingPrepare->reqId, placeholders::_1, placeholders::_2));

  pendingPrepare->requestTimeout->Reset();
}

void ShardClient::Commit(uint64_t id, const Transaction & txn,
      uint64_t timestamp, commit_callback ccb, commit_timeout_callback ctcb,
      uint32_t timeout) {
  

  // create commit request
  string request_str;
  Request request;
  request.set_op(Request::COMMIT);
  request.set_txnid(id);
  request.mutable_commit()->set_timestamp(timestamp);
  request.SerializeToString(&request_str);

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

  client->InvokeInconsistent(request_str, bind(&ShardClient::CommitCallback,
      this, pendingCommit->reqId, placeholders::_1, placeholders::_2));
  Debug("[shard %i] Sent COMMIT [%lu]", shard, id);

  pendingCommit->requestTimeout->Reset();
}  
  
void ShardClient::Abort(uint64_t id, const Transaction &txn,
      abort_callback acb, abort_timeout_callback atcb, uint32_t timeout) {
  Debug("[shard %i] Sending ABORT [%lu]", shard, id);

  // create abort request
  string request_str;
  Request request;
  request.set_op(Request::ABORT);
  request.set_txnid(id);
  txn.serialize(request.mutable_abort()->mutable_txn());
  request.SerializeToString(&request_str);

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

  client->InvokeInconsistent(request_str, bind(&ShardClient::AbortCallback,
      this, pendingAbort->reqId, placeholders::_1, placeholders::_2));

  pendingAbort->requestTimeout->Reset();
}

std::string ShardClient::TapirDecide(const std::map<std::string,std::size_t> &results) {
  // If a majority say prepare_ok,
  int ok_count = 0;
  Timestamp ts = 0;
  string final_reply_str;
  Reply final_reply;

  for (const auto& string_and_count : results) {
    const std::string &s = string_and_count.first;
    const std::size_t count = string_and_count.second;

    Reply reply;
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
void ShardClient::GetCallback(uint64_t reqId, const string &request_str,
    const string &reply_str) {
  /* Replies back from a shard. */
  Reply reply;
  reply.ParseFromString(reply_str);

  auto itr = this->pendingGets.find(reqId);
  if (itr != this->pendingGets.end()) {
    get_callback gcb = itr->second->gcb;
    std::string key = itr->second->key;
    this->pendingGets.erase(itr);
    delete itr->second;
    Debug("[shard %lu:%i] GET callback [%d]", client_id, shard, reply.status());
    if (reply.has_timestamp()) {
      gcb(reply.status(), key, reply.value(), Timestamp(reply.timestamp()));
    } else {
      gcb(reply.status(), key, reply.value(), Timestamp());
    }
  }
}

/* Callback from a shard replica on prepare operation completion. */
void ShardClient::PrepareCallback(uint64_t reqId,
    const string &request_str, const string &reply_str) {
  Reply reply;
  reply.ParseFromString(reply_str);

  Debug("[shard %lu:%i] PREPARE callback [%d]", client_id, shard, reply.status());

  auto itr = this->pendingPrepares.find(reqId);
  if (itr != this->pendingPrepares.end()) {
    prepare_callback pcb = itr->second->pcb;
    this->pendingPrepares.erase(itr);
    delete itr->second;
    if (reply.has_timestamp()) {
      pcb(reply.status(), Timestamp(reply.timestamp()));
    } else {
      pcb(reply.status(), Timestamp());
    }
  }
}

/* Callback from a shard replica on commit operation completion. */
void ShardClient::CommitCallback(uint64_t reqId,
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
}

/* Callback from a shard replica on abort operation completion. */
void ShardClient::AbortCallback(uint64_t reqId,
    const string &request_str, const string &reply_str) {
  // ABORTs always succeed.

  // ASSERT(blockingBegin != NULL);
  // blockingBegin->Reply(0);

  Debug("[shard %lu:%i] ABORT callback", client_id, shard);

  auto itr = this->pendingAborts.find(reqId);
  if (itr != this->pendingAborts.end()) {
    abort_callback acb = itr->second->acb;
    this->pendingAborts.erase(itr);
    delete itr->second;
    acb();
  }

}

} // namespace tapir
