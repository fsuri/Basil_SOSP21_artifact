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
    uint64_t readQuorumSize) : client_id(client_id),
    transport(transport), config(config), shard(shard),
    readQuorumSize(readQuorumSize), signedMessages(false),
    validateProofs(false), cryptoConfig(nullptr) {
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
  proto::Prepare1Reply prepare1Reply;
  
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
  } else if (type == prepare1Reply.GetTypeName()) {
    prepare1Reply.ParseFromString(data);
    HandlePrepare1Reply(prepare1Reply);
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
  proto::Prepare1 prepare;
  prepare.set_req_id(reqId);
  prepare.set_txn_id(id);
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
  // reply has already been validated as sent by the expected replica
  auto itr = this->pendingGets.find(reply.req_id());
  if (itr == this->pendingGets.end()) {
    return; // this is a stale request
  }

  if (validateProofs) {
    // Verify the the p3 commits the given tx, that the tx version matches the
    // read version, and that the tx writes the key
    if (!VerifyP3Commit(reply.proof().write_txn(), reply.proof().prepare3())) {
      return;
    }
    if (!VersionsEqual(reply.proof().write_txn_version(), reply.version())) {
      return;
    }
    if (!TxWritesKey(reply.proof().write_txn(), reply.key())) {
      return;
    }
  }

  // value and timestamp are valid
  itr->second->numReplies++;
  if (reply.status() == REPLY_OK) {
    itr->second->numOKReplies++;
    Timestamp replyTs(reply.version().timestamp());
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
void ShardClient::HandlePrepare1Reply(const proto::Prepare1Reply &reply) {
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

bool ShardClient::VerifyP3Commit(const Transaction &transaction,
    const proto::Prepare3 &p3) {
  TransactionMessage txn_msg;
  transaction.serialize(&txn_msg);
  std::string serialized = txn_msg.SerializeAsString();
  std::string txdigest = crypto::Hash(serialized);

  for (int i = 0; i < p3.p2_replies_size(); i++) {
    proto::SignedPrepare2Reply signedPrepare2Reply = p3.p2_replies(i);
    proto::Prepare2Reply prepare2Reply = signedPrepare2Reply.p2_reply();
    std::string prepare2ReplySerialized = prepare2Reply.SerializeAsString();
    uint64_t replicaId = signedPrepare2Reply.replica_id();
    crypto::PubKey replicaPublicKey = cryptoConfig->getReplicaPublicKey(replicaId);
    if (crypto::IsMessageValid(replicaPublicKey, prepare2ReplySerialized,
          &signedPrepare2Reply) && prepare2Reply.txdigest() == txdigest &&
        prepare2Reply.action() == proto::TxnAction::COMMIT) {
      // pass
    } else {
      return false;
    }

  }
  return true;
}

bool ShardClient::TxWritesKey(const Transaction &tx, const std::string &key) {
  return tx.getWriteSet().find(key) != tx.getWriteSet().end();
}

bool ShardClient::VersionsEqual(const proto::Version &v1, const proto::Version &v2) {
  return Timestamp(v1.timestamp()) == Timestamp(v2.timestamp()) && v1.clientid() == v2.clientid();
}

bool ShardClient::VersionGT(const proto::Version &v1, const proto::Version &v2) {
  return Timestamp(v1.timestamp()) > Timestamp(v2.timestamp()) || (Timestamp(v1.timestamp()) == Timestamp(v2.timestamp()) && v1.clientid() > v2.clientid());
}

} // namespace indicus
