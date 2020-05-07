// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * store/indicusstore/groupclient.cc:
 *   Single group indicus transactional client.
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

#include <google/protobuf/util/message_differencer.h>

#include "store/indicusstore/common.h"

namespace indicusstore {

ShardClient::ShardClient(transport::Configuration *config, Transport *transport,
    uint64_t client_id, int group, const std::vector<int> &closestReplicas_,
    bool signedMessages, bool validateProofs, bool hashDigest,
    KeyManager *keyManager, TrueTime &timeServer) :
    client_id(client_id), transport(transport), config(config), group(group),
    timeServer(timeServer),
    signedMessages(signedMessages), validateProofs(validateProofs),
    hashDigest(hashDigest),
    keyManager(keyManager), phase1DecisionTimeout(1000UL), lastReqId(0UL) {
  transport->Register(this, *config, -1, -1);

  if (closestReplicas_.size() == 0) {
    for  (int i = 0; i < config->n; ++i) {
      closestReplicas.push_back((i + client_id) % config->n);
    }
  } else {
    closestReplicas = closestReplicas;
  }
}

ShardClient::~ShardClient() {
}

void ShardClient::ReceiveMessage(const TransportAddress &remote,
      const std::string &type, const std::string &data, void *meta_data) {
  proto::SignedMessage signedMessage;
  proto::ReadReply readReply;
  proto::Phase1Reply phase1Reply;
  proto::Phase2Reply phase2Reply;

  if (type == readReply.GetTypeName()) {
    readReply.ParseFromString(data);
    HandleReadReply(readReply);
  } else if (type == phase1Reply.GetTypeName()) {
    phase1Reply.ParseFromString(data);
    HandlePhase1Reply(phase1Reply);
  } else if (type == phase2Reply.GetTypeName()) {
    phase2Reply.ParseFromString(data);
    HandlePhase2Reply(phase2Reply);
  } else {
    Panic("Received unexpected message type: %s", type.c_str());
  }
}

void ShardClient::Begin(uint64_t id) {
  Debug("[group %i] BEGIN: %lu", group, id);

  txn = proto::Transaction();
  readValues.clear();
}

void ShardClient::Get(uint64_t id, const std::string &key,
    const TimestampMessage &ts, uint64_t readMessages, uint64_t rqs,
    uint64_t rds, read_callback gcb, read_timeout_callback gtcb,
    uint32_t timeout) {
  if (BufferGet(key, gcb)) {
    Debug("[group %i] read from buffer.", group);
    return;
  }

  uint64_t reqId = lastReqId++;
  PendingQuorumGet *pendingGet = new PendingQuorumGet(reqId);
  pendingGets[reqId] = pendingGet;
  pendingGet->key = key;
  pendingGet->rqs = rqs;
  pendingGet->rds = rds;
  pendingGet->gcb = gcb;
  pendingGet->gtcb = gtcb;

  proto::Read read;
  read.set_req_id(reqId);
  read.set_key(key);
  *read.mutable_timestamp() = ts;

  UW_ASSERT(rqs <= closestReplicas.size());
  for (size_t i = 0; i < readMessages; ++i) {
    Debug("[group %i] Sending GET to replica %d", group, closestReplicas[i]);
    transport->SendMessageToReplica(this, group, closestReplicas[i], read);
  }

  Debug("[group %i] Sent GET [%lu : %lu]", group, id, reqId);
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
    const std::string &txnDigest,
    phase1_callback pcb, phase1_timeout_callback ptcb, uint32_t timeout) {
  Debug("[group %i] Sending PHASE1 [%lu]", group, id);
  uint64_t reqId = lastReqId++;
  PendingPhase1 *pendingPhase1 = new PendingPhase1(reqId, group, transaction,
      txnDigest, config, keyManager, validateProofs, signedMessages, hashDigest);
  pendingPhase1s[reqId] = pendingPhase1;
  pendingPhase1->pcb = pcb;
  pendingPhase1->ptcb = ptcb;
  pendingPhase1->requestTimeout = new Timeout(transport, timeout, [this, pendingPhase1]() {
      phase1_timeout_callback ptcb = pendingPhase1->ptcb;
      auto itr = this->pendingPhase1s.find(pendingPhase1->reqId);
      if (itr != this->pendingPhase1s.end()) {
        PendingPhase1 *pendingPhase1 = itr->second;
        this->pendingPhase1s.erase(itr);
        delete pendingPhase1;
      }
      ptcb(REPLY_TIMEOUT);
  });

  // create prepare request
  proto::Phase1 phase1;
  phase1.set_req_id(reqId);
  *phase1.mutable_txn() = transaction;

  transport->SendMessageToGroup(this, group, phase1);

  pendingPhase1->requestTimeout->Reset();
}

void ShardClient::Phase2(uint64_t id,
    const proto::Transaction &txn,
    proto::CommitDecision decision,
    const proto::GroupedSignatures &groupedSigs, phase2_callback pcb,
    phase2_timeout_callback ptcb, uint32_t timeout) {
  Debug("[group %i] Sending PHASE2 [%lu]", group, id);
  uint64_t reqId = lastReqId++;
  PendingPhase2 *pendingPhase2 = new PendingPhase2(reqId, decision);
  pendingPhase2s[reqId] = pendingPhase2;
  pendingPhase2->pcb = pcb;
  pendingPhase2->ptcb = ptcb;
  pendingPhase2->requestTimeout = new Timeout(transport, timeout, [this, pendingPhase2]() {
      phase2_timeout_callback ptcb = pendingPhase2->ptcb;
      auto itr = this->pendingPhase2s.find(pendingPhase2->reqId);
      if (itr != this->pendingPhase2s.end()) {
        PendingPhase2 *pendingPhase2 = itr->second;
        this->pendingPhase2s.erase(itr);
        delete pendingPhase2;
      }

      ptcb(REPLY_TIMEOUT);
  });

  proto::Phase2 phase2;
  phase2.set_req_id(reqId);
  phase2.set_decision(decision);
  if (validateProofs) {
    *phase2.mutable_txn() = txn;
    if (signedMessages) {
      *phase2.mutable_grouped_sigs() = groupedSigs;
    }
  }
  transport->SendMessageToGroup(this, group, phase2);

  pendingPhase2->requestTimeout->Reset();
}

void ShardClient::Writeback(uint64_t id, const proto::Transaction &transaction,
    const std::string &txnDigest,
    proto::CommitDecision decision, bool fast, const proto::CommittedProof &conflict,
    const proto::GroupedSignatures &p1Sigs, const proto::GroupedSignatures &p2Sigs) {

  // create commit request
  proto::Writeback writeback;
  writeback.set_decision(decision);
  if (validateProofs && signedMessages) {
    if (fast && decision == proto::COMMIT) {
      *writeback.mutable_p1_sigs() = p1Sigs;
    } else if (fast && decision == proto::ABORT) {
      *writeback.mutable_conflict() = conflict;
    } else {
      *writeback.mutable_p2_sigs() = p2Sigs;
    }
  }
  if (decision == proto::COMMIT) {
    *writeback.mutable_txn() = transaction;
  }
  writeback.set_txn_digest(txnDigest);
  
  transport->SendMessageToGroup(this, group, writeback);
  Debug("[group %i] Sent WRITEBACK[%lu]", group, id);
}

void ShardClient::Abort(uint64_t id, const TimestampMessage &ts) {
  proto::Abort abort;
  *abort.mutable_internal()->mutable_ts() = ts;
  for (const auto &read : txn.read_set()) {
    *abort.mutable_internal()->add_read_set() = read.key();
  }

  if (validateProofs && signedMessages) {
    proto::AbortInternal internal(abort.internal());
    SignMessage(internal, keyManager->GetPrivateKey(client_id), client_id,
        abort.mutable_signed_internal());
  }

  transport->SendMessageToGroup(this, group, abort);

  Debug("[group %i] Sent ABORT[%lu]", group, id);
}

bool ShardClient::BufferGet(const std::string &key, read_callback rcb) {
  for (const auto &write : txn.write_set()) {
    if (write.key() == key) {
      Debug("[group %i] Key %s was written with val %s.", group,
          BytesToHex(key, 16).c_str(), BytesToHex(write.value(), 16).c_str());
      rcb(REPLY_OK, key, write.value(), Timestamp(), proto::Dependency(),
          false, false);
      return true;
    }
  }

  for (const auto &read : txn.read_set()) {
    if (read.key() == key) {
      Debug("[group %i] Key %s was already read with ts %lu.%lu.", group,
          BytesToHex(key, 16).c_str(), read.readtime().timestamp(),
          read.readtime().id());
      rcb(REPLY_OK, key, readValues[key], read.readtime(), proto::Dependency(),
          false, false);
      return true;
    }
  }

  return false;
}

void ShardClient::GetTimeout(uint64_t reqId) {
  auto itr = this->pendingGets.find(reqId);
  if (itr != this->pendingGets.end()) {
    PendingQuorumGet *pendingGet = itr->second;
    get_timeout_callback gtcb = pendingGet->gtcb;
    std::string key = pendingGet->key;
    this->pendingGets.erase(itr);
    delete pendingGet;
    gtcb(REPLY_TIMEOUT, key);
  }
}

/* Callback from a group replica on get operation completion. */
void ShardClient::HandleReadReply(const proto::ReadReply &reply) {
  auto itr = this->pendingGets.find(reply.req_id());
  if (itr == this->pendingGets.end()) {
    return; // this is a stale request
  }
  PendingQuorumGet *req = itr->second;
  Debug("[group %i] ReadReply for %lu.", group, reply.req_id());

  // value and timestamp are valid
  req->numReplies++;
  if (reply.has_committed()) {
    if (validateProofs) {
      std::string committedTxnDigest = TransactionDigest(reply.committed().proof().txn(), hashDigest);
      if (!ValidateTransactionWrite(reply.committed().proof(), &committedTxnDigest,
            req->key, reply.committed().value(), reply.committed().timestamp(), config,
            signedMessages, keyManager)) {
        Debug("[group %i] Failed to validate committed value for read %lu.",
            group, reply.req_id());
        // invalid replies can be treated as if we never received a reply from
        //     a crashed replica
        return;
      }
    }

    Timestamp replyTs(reply.committed().timestamp());
    Debug("[group %i] ReadReply for %lu with committed %lu byte value and ts"
        " %lu.%lu.", group, reply.req_id(), reply.committed().value().length(),
        replyTs.getTimestamp(), replyTs.getID());
    if (req->firstCommittedReply || req->maxTs < replyTs) {
      req->maxTs = replyTs;
      req->maxValue = reply.committed().value();
    }
    req->firstCommittedReply = false;
  }

  const proto::PreparedWrite *prepared;
  proto::PreparedWrite validatedPrepared;
  bool hasPrepared = false;
  if (validateProofs && signedMessages) {
    if (reply.has_signed_prepared()) {
      if (!crypto::Verify(keyManager->GetPublicKey(
              reply.signed_prepared().process_id()),
              reply.signed_prepared().data(),
              reply.signed_prepared().signature())) {
          return;
      }
      
      if(!validatedPrepared.ParseFromString(reply.signed_prepared().data())) {
        return;
      }

      prepared = &validatedPrepared;
      hasPrepared = true;
    }
  } else {
    if (reply.has_prepared()) {
      hasPrepared = true;
      prepared = &reply.prepared();
    }
  }

  if (hasPrepared) {
    Timestamp preparedTs(prepared->timestamp());
    Debug("[group %i] ReadReply for %lu with prepared %lu byte value and ts"
        " %lu.%lu.", group, reply.req_id(), prepared->value().length(),
        preparedTs.getTimestamp(), preparedTs.getID());
    auto preparedItr = req->prepared.find(preparedTs);
    if (preparedItr == req->prepared.end()) {
      req->prepared.insert(std::make_pair(preparedTs,
            std::make_pair(*prepared, 1)));
    } else if (preparedItr->second.first == *prepared) {
      preparedItr->second.second += 1;
    }

    if (validateProofs && signedMessages) {
      proto::Signature *sig = req->preparedSigs[preparedTs].add_sigs();
      sig->set_process_id(reply.signed_prepared().process_id());
      *sig->mutable_signature() = reply.signed_prepared().signature();
    }
  }

  if (req->numReplies >= req->rqs) {
    for (auto preparedItr = req->prepared.rbegin();
        preparedItr != req->prepared.rend(); ++preparedItr) {
      if (preparedItr->first < req->maxTs) {
        break;
      }

      if (preparedItr->second.second >= req->rds) {
        req->maxTs = preparedItr->first;
        req->maxValue = preparedItr->second.first.value();
        *req->dep.mutable_prepared() = preparedItr->second.first;
        if (validateProofs && signedMessages) {
          *req->dep.mutable_prepared_sigs() = req->preparedSigs[preparedItr->first];
        }
        req->dep.set_involved_group(group);
        req->hasDep = true;
        break;
      }
    }
    pendingGets.erase(itr);
    ReadMessage *read = txn.add_read_set();
    *read->mutable_key() = req->key;
    req->maxTs.serialize(read->mutable_readtime());
    readValues[req->key] = req->maxValue;
    req->gcb(REPLY_OK, req->key, req->maxValue, req->maxTs, req->dep,
        req->hasDep, true);
    delete req;
  }
}

void ShardClient::HandlePhase1Reply(const proto::Phase1Reply &reply) {
  auto itr = this->pendingPhase1s.find(reply.req_id());
  if (itr == this->pendingPhase1s.end()) {
    return; // this is a stale request
  }

  PendingPhase1 *pendingPhase1 = itr->second;

  bool hasSigned = (validateProofs && signedMessages) && (!reply.has_cc() || reply.cc().ccr() != proto::ConcurrencyControl::ABORT);

  const proto::ConcurrencyControl *cc = nullptr;
  proto::ConcurrencyControl validatedCC;
  if (hasSigned) {
    Debug("Verifying signed_cc because has_cc %d and ccr %d.", reply.has_cc(),
        reply.cc().ccr());
    if (!reply.has_signed_cc()) {
      return;
    }

    if (!IsReplicaInGroup(reply.signed_cc().process_id(), group, config)) {
      Debug("[group %d] Phase1Reply from replica %lu who is not in group.",
          group, reply.signed_cc().process_id());
      return;
    }

    if (!crypto::Verify(keyManager->GetPublicKey(reply.signed_cc().process_id()),
          reply.signed_cc().data(), reply.signed_cc().signature())) {
      return;
    }

    if (!validatedCC.ParseFromString(reply.signed_cc().data())) {
      return;
    }

    cc = &validatedCC;
  } else {
    UW_ASSERT(reply.has_cc());

    cc = &reply.cc();
  }
  
  Debug("[group %i] PHASE1 callback ccr=%d", group, cc->ccr());

  if (!pendingPhase1->p1Validator.ProcessMessage(*cc)) {
    return;
  }

  if (hasSigned) {
    proto::Signature *sig = pendingPhase1->p1ReplySigs[cc->ccr()].add_sigs();
    sig->set_process_id(reply.signed_cc().process_id());
    *sig->mutable_signature() = reply.signed_cc().signature();
  }

  Phase1ValidationState state = pendingPhase1->p1Validator.GetState();
  switch (state) {
    case FAST_COMMIT:
      pendingPhase1->decision = proto::COMMIT;
      pendingPhase1->fast = true;
      Phase1Decision(itr);
      break;
    case FAST_ABORT:
      pendingPhase1->decision = proto::ABORT;
      pendingPhase1->fast = true;
      if (validateProofs) {
        pendingPhase1->conflict = cc->committed_conflict();
      }
      Phase1Decision(itr);
      break;
    case SLOW_COMMIT_FINAL:
      pendingPhase1->decision = proto::COMMIT;
      pendingPhase1->fast = false;
      Phase1Decision(itr);
      break;
    case SLOW_ABORT_FINAL:
      pendingPhase1->decision = proto::ABORT;
      pendingPhase1->fast = false;
      Phase1Decision(itr);
      break;
    case SLOW_COMMIT_TENTATIVE:
      if (!pendingPhase1->decisionTimeoutStarted) {
        uint64_t reqId = reply.req_id();
        pendingPhase1->decisionTimeout = new Timeout(transport,
            phase1DecisionTimeout, [this, reqId]() {
              auto itr = pendingPhase1s.find(reqId);
              if (itr == pendingPhase1s.end()) {
                return;
              }
              itr->second->decision = proto::COMMIT;
              itr->second->fast = false;
              Phase1Decision(itr);      
            }
          );
        pendingPhase1->decisionTimeout->Reset();
        pendingPhase1->decisionTimeoutStarted = true;
      }
      break;

    case SLOW_ABORT_TENTATIVE:
      if (!pendingPhase1->decisionTimeoutStarted) {
        uint64_t reqId = reply.req_id();
        pendingPhase1->decisionTimeout = new Timeout(transport,
            phase1DecisionTimeout, [this, reqId]() {
              auto itr = pendingPhase1s.find(reqId);
              if (itr == pendingPhase1s.end()) {
                return;
              }
              itr->second->decision = proto::ABORT;
              itr->second->fast = false;
              Phase1Decision(itr);      
            }
          );
        pendingPhase1->decisionTimeout->Reset();
        pendingPhase1->decisionTimeoutStarted = true;
      }
      break;
    case NOT_ENOUGH:
      break;
    default:
      break;
  }
}

void ShardClient::HandlePhase2Reply(const proto::Phase2Reply &reply) {
  auto itr = this->pendingPhase2s.find(reply.req_id());
  if (itr == this->pendingPhase2s.end()) {
    Debug("[group %i] Received stale Phase2Reply for request %lu.", group,
        reply.req_id());
    return; // this is a stale request
  }

  const proto::Phase2Decision *p2Decision = nullptr;
  proto::Phase2Decision validatedP2Decision;
  if (validateProofs && signedMessages) {
    if (!reply.has_signed_p2_decision()) {
      Debug("[group %i] Phase2Reply missing signed_p2_decision.", group);
      return;
    }

    if (!IsReplicaInGroup(reply.signed_p2_decision().process_id(), group, config)) {
      Debug("[group %d] Phase2Reply from replica %lu who is not in group.",
          group, reply.signed_p2_decision().process_id());
      return;
    }

    if (!crypto::Verify(keyManager->GetPublicKey(
            reply.signed_p2_decision().process_id()),
          reply.signed_p2_decision().data(),
          reply.signed_p2_decision().signature())) {
      return;
    }

    if (!validatedP2Decision.ParseFromString(reply.signed_p2_decision().data())) {
      return;
    }

    p2Decision = &validatedP2Decision;

  } else {
    p2Decision = &reply.p2_decision();
  }

  Debug("[group %i] PHASE2 reply with decision %d", group,
      p2Decision->decision());

  if (validateProofs && signedMessages) {
    proto::Signature *sig = itr->second->p2ReplySigs.add_sigs();
    sig->set_process_id(reply.signed_p2_decision().process_id());
    *sig->mutable_signature()= reply.signed_p2_decision().signature();
  }

  if (p2Decision->decision() == itr->second->decision) {
    itr->second->matchingReplies++;
  }

  if (itr->second->matchingReplies >= QuorumSize(config)) {
    PendingPhase2 *pendingPhase2 = itr->second;
    pendingPhase2->pcb(pendingPhase2->p2ReplySigs);
    this->pendingPhase2s.erase(itr);
    delete pendingPhase2;
  }
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
  PendingPhase1 *pendingPhase1 = itr->second;
  pendingPhase1->pcb(pendingPhase1->decision, pendingPhase1->fast,
      pendingPhase1->conflict, pendingPhase1->p1ReplySigs);
  this->pendingPhase1s.erase(itr);
  delete pendingPhase1;
}

} // namespace indicus
