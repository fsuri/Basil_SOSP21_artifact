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
    bool pingReplicas,
    Parameters params, KeyManager *keyManager, Verifier *verifier,
    TrueTime &timeServer) :
    PingInitiator(this, transport, config->n),
    client_id(client_id), transport(transport), config(config), group(group),
    timeServer(timeServer), pingReplicas(pingReplicas), params(params),
    keyManager(keyManager), verifier(verifier), phase1DecisionTimeout(1000UL),
    lastReqId(0UL) {
  transport->Register(this, *config, -1, -1);

  if (closestReplicas_.size() == 0) {
    for  (int i = 0; i < config->n; ++i) {
      closestReplicas.push_back((i + client_id) % config->n);
      // Debug("i: %d; client_id: %d", i, client_id);
      // Debug("Calculations: %d", (i + client_id) % config->n);
    }
  } else {
    closestReplicas = closestReplicas_;
  }
}

ShardClient::~ShardClient() {
}

void ShardClient::ReceiveMessage(const TransportAddress &remote,
      const std::string &type, const std::string &data, void *meta_data) {
  if (type == readReply.GetTypeName()) {
    readReply.ParseFromString(data);
    HandleReadReply(readReply);
  } else if (type == phase1Reply.GetTypeName()) {
    phase1Reply.ParseFromString(data);
    HandlePhase1Reply(phase1Reply);
  } else if (type == phase2Reply.GetTypeName()) {
    phase2Reply.ParseFromString(data);
    HandlePhase2Reply(phase2Reply);
  } else if (type == ping.GetTypeName()) {
    ping.ParseFromString(data);
    HandlePingResponse(ping);
    //FALLBACK readMessages
  } else if(type == relayP1.GetTypeName()){ //receive full TX info for a dependency
    relayP1.ParseFromString(data);
    HandlePhase1Relay(relayP1); //Call into client to see if still waiting.

  }
  else if(type == phase1FBReply.GetTypeName()){
    //wait for quorum and relay to client
    phase1FBReply.ParseFromString(data);
    HandlePhase1FBReply(phase1FBReply); // update pendingFB state -- if complete, upcall to client
  }
  else if(type == phase2FBReply.GetTypeName()){
    //wait for quorum and relay to client
    phase2FBReply.ParseFromString(data);
    HandlePhase2FBReply(phase2FBReply); //update pendingFB state -- if complete, upcall to client
  }
  else {
    Panic("Received unexpected message type: %s", type.c_str());
  }
}

void ShardClient::Begin(uint64_t id) {
  Debug("[group %i] BEGIN: %lu", group, id);

  txn.Clear();
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

  read.Clear();
  read.set_req_id(reqId);
  read.set_key(key);
  *read.mutable_timestamp() = ts;

  UW_ASSERT(readMessages <= closestReplicas.size());
  for (size_t i = 0; i < readMessages; ++i) {
    Debug("[group %i] Sending GET to replica %lu", group, GetNthClosestReplica(i));
    transport->SendMessageToReplica(this, group, GetNthClosestReplica(i), read);
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



void ShardClient::Phase1(uint64_t id, const proto::Transaction &transaction, const std::string &txnDigest,
  phase1_callback pcb, phase1_timeout_callback ptcb, relayP1_callback rcb, uint32_t timeout) {
  Debug("[group %i] Sending PHASE1 [%lu]", group, id);
  uint64_t reqId = lastReqId++;
  PendingPhase1 *pendingPhase1 = new PendingPhase1(reqId, group, transaction,
      txnDigest, config, keyManager, params, verifier);
  pendingPhase1s[reqId] = pendingPhase1;
  pendingPhase1->pcb = pcb;
  pendingPhase1->ptcb = ptcb;
  pendingPhase1->rcb = rcb;
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
  phase1.Clear();
  phase1.set_req_id(reqId);
  *phase1.mutable_txn() = transaction;

  transport->SendMessageToGroup(this, group, phase1);

  pendingPhase1->requestTimeout->Reset();
}

void ShardClient::Phase2(uint64_t id,
    const proto::Transaction &txn, const std::string &txnDigest,
    proto::CommitDecision decision,
    const proto::GroupedSignatures &groupedSigs, phase2_callback pcb,
    phase2_timeout_callback ptcb, uint32_t timeout) {
  Debug("[group %i] Sending PHASE2 [%lu]", group, id);
  uint64_t reqId = lastReqId++;
  PendingPhase2 *pendingPhase2 = new PendingPhase2(reqId, decision);  //TODO: add view that this decision is from (default = 0).
  //TODO: When sending an InvokeFB message, this view = the view you propose ; but unclear what decision you are waiting for?
  //Create many mappings for potential views/decisions instead.
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

  phase2.Clear();
  phase2.set_req_id(reqId);
  phase2.set_decision(decision);
  *phase2.mutable_txn_digest() = txnDigest;
  if (params.validateProofs && params.signedMessages) {
    *phase2.mutable_grouped_sigs() = groupedSigs;
  }
  transport->SendMessageToGroup(this, group, phase2);

  pendingPhase2->requestTimeout->Reset();
}

void ShardClient::Writeback(uint64_t id, const proto::Transaction &transaction, const std::string &txnDigest,
  proto::CommitDecision decision, bool fast, const proto::CommittedProof &conflict,
  const proto::GroupedSignatures &p1Sigs, const proto::GroupedSignatures &p2Sigs) {

  writeback.Clear();
  // create commit request
  writeback.set_decision(decision);
  if (params.validateProofs && params.signedMessages) {
    if (fast && decision == proto::COMMIT) {
      *writeback.mutable_p1_sigs() = p1Sigs;
    } else if (fast && decision == proto::ABORT) {
      *writeback.mutable_conflict() = conflict;
      if(conflict.has_p2_view()){
        writeback.set_p2_view(conflict.p2_view());
      }
      else{
        writeback.set_p2_view(0); //implies that this was a p1 proof for the conflict, attaching a view anyway..
      }

    } else {
      *writeback.mutable_p2_sigs() = p2Sigs;
      writeback.set_p2_view(0); //TODO: extend this to process other views too? Bookkeeping should only be needed
      // for fallback though. Either combine the logic, or change it so that the orignial client issues FB function too

    }
  }
  writeback.set_txn_digest(txnDigest);

  transport->SendMessageToGroup(this, group, writeback);
  Debug("[group %i] Sent WRITEBACK[%lu]", group, id);
}

//Overloaded Wb function to not include ID, this is purely for debug purpose to distinguish whether a message came from FB instance.
void ShardClient::WritebackFB(const proto::Transaction &transaction,
    const std::string &txnDigest,
    proto::CommitDecision decision, bool fast, const proto::CommittedProof &conflict,
    const proto::GroupedSignatures &p1Sigs, const proto::GroupedSignatures &p2Sigs) {

  writeback.Clear();
  // create commit request
  writeback.set_decision(decision);
  if (params.validateProofs && params.signedMessages) {
    if (fast && decision == proto::COMMIT) {
      *writeback.mutable_p1_sigs() = p1Sigs;
    } else if (fast && decision == proto::ABORT) {
      *writeback.mutable_conflict() = conflict;
    } else {
      *writeback.mutable_p2_sigs() = p2Sigs;
    }
  }
  writeback.set_txn_digest(txnDigest);

  transport->SendMessageToGroup(this, group, writeback);
  Debug("[group %i] Sent Fallback WRITEBACK[%s]", group, txnDigest.c_str());

  //TODO:delete pendingFB instance? And all pending phase2 that are inside.
}


void ShardClient::Abort(uint64_t id, const TimestampMessage &ts) {
  abort.Clear();
  *abort.mutable_internal()->mutable_ts() = ts;
  for (const auto &read : txn.read_set()) {
    *abort.mutable_internal()->add_read_set() = read.key();
  }

  if (params.validateProofs && params.signedMessages) {
    proto::AbortInternal internal(abort.internal());
    if (params.signatureBatchSize == 1) {
      SignMessage(&internal, keyManager->GetPrivateKey(client_id % 1024), client_id % 1024,
          abort.mutable_signed_internal());
    } else {
      std::vector<::google::protobuf::Message*> messages = {&internal};
      std::vector<proto::SignedMessage*> signedMessages = {abort.mutable_signed_internal()};
      SignMessages(messages, keyManager->GetPrivateKey(client_id % 1024), client_id % 1024,
          signedMessages, params.merkleBranchFactor);
    }
  }

  transport->SendMessageToGroup(this, group, abort);

  Debug("[group %i] Sent ABORT[%lu]", group, id);
}

bool ShardClient::SendPing(size_t replica, const PingMessage &ping) {
  transport->SendMessageToReplica(this, group, replica, ping);
  return true;
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

  const proto::Write *write;
  if (params.validateProofs && params.signedMessages) {
    if (reply.has_signed_write()) {
      if (!verifier->Verify(keyManager->GetPublicKey(reply.signed_write().process_id()),
              reply.signed_write().data(), reply.signed_write().signature())) {
        Debug("[group %i] Failed to validate signature for write.", group);
        return;
      }

      if(!validatedPrepared.ParseFromString(reply.signed_write().data())) {
        Debug("[group %i] Invalid serialization of write.", group);
        return;
      }

      write = &validatedPrepared;
    } else {
      if (reply.has_write() && reply.write().has_committed_value()) {
        Debug("[group %i] Reply contains unsigned committed value.", group);
        return;
      }

      if (params.verifyDeps && reply.has_write() && reply.write().has_prepared_value()) {
        Debug("[group %i] Reply contains unsigned prepared value.", group);
        return;
      }

      write = &reply.write();
      UW_ASSERT(!write->has_committed_value());
      UW_ASSERT(!write->has_prepared_value() || !params.verifyDeps);
    }
  } else {
    write = &reply.write();
  }

  // value and timestamp are valid
  req->numReplies++;
  if (write->has_committed_value() && write->has_committed_timestamp()) {
    if (params.validateProofs) {
      if (!reply.has_proof()) {
        Debug("[group %i] Missing proof for committed write.", group);
        return;
      }

      std::string committedTxnDigest = TransactionDigest(
          reply.proof().txn(), params.hashDigest);
      if (!ValidateTransactionWrite(reply.proof(), &committedTxnDigest,
            req->key, write->committed_value(), write->committed_timestamp(),
            config, params.signedMessages, keyManager, verifier)) {
        Debug("[group %i] Failed to validate committed value for read %lu.",
            group, reply.req_id());
        // invalid replies can be treated as if we never received a reply from
        //     a crashed replica
        return;
      }
    }

    Timestamp replyTs(write->committed_timestamp());
    Debug("[group %i] ReadReply for %lu with committed %lu byte value and ts"
        " %lu.%lu.", group, reply.req_id(), write->committed_value().length(),
        replyTs.getTimestamp(), replyTs.getID());
    if (req->firstCommittedReply || req->maxTs < replyTs) {
      req->maxTs = replyTs;
      req->maxValue = write->committed_value();
    }
    req->firstCommittedReply = false;
  }

  if (params.maxDepDepth > -2 && write->has_prepared_value() &&
      write->has_prepared_timestamp() &&
      write->has_prepared_txn_digest()) {
    Timestamp preparedTs(write->prepared_timestamp());
    Debug("[group %i] ReadReply for %lu with prepared %lu byte value and ts"
        " %lu.%lu.", group, reply.req_id(), write->prepared_value().length(),
        preparedTs.getTimestamp(), preparedTs.getID());
    auto preparedItr = req->prepared.find(preparedTs);
    if (preparedItr == req->prepared.end()) {
      req->prepared.insert(std::make_pair(preparedTs,
            std::make_pair(*write, 1)));
    } else if (preparedItr->second.first == *write) {
      preparedItr->second.second += 1;
    }

    if (params.validateProofs && params.signedMessages && params.verifyDeps) {
      proto::Signature *sig = req->preparedSigs[preparedTs].add_sigs();
      sig->set_process_id(reply.signed_write().process_id());
      *sig->mutable_signature() = reply.signed_write().signature();
    }
  }

  if (req->numReplies >= req->rqs) {
    if (params.maxDepDepth > -2) {
      for (auto preparedItr = req->prepared.rbegin();
          preparedItr != req->prepared.rend(); ++preparedItr) {
        if (preparedItr->first < req->maxTs) {
          break;
        }

        if (preparedItr->second.second >= req->rds) {
          req->maxTs = preparedItr->first;
          req->maxValue = preparedItr->second.first.prepared_value();
          *req->dep.mutable_write() = preparedItr->second.first;
          if (params.validateProofs && params.signedMessages && params.verifyDeps) {
            *req->dep.mutable_write_sigs() = req->preparedSigs[preparedItr->first];
          }
          req->dep.set_involved_group(group);
          req->hasDep = true;
          break;
        }
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

  bool hasSigned = (params.validateProofs && params.signedMessages) &&
   (!reply.has_cc() || reply.cc().ccr() != proto::ConcurrencyControl::ABORT);

  const proto::ConcurrencyControl *cc = nullptr;
  if (hasSigned) {
    Debug("[group %i] Verifying signed_cc from %lu with signatures bytes %lu"
        " because has_cc %d and ccr %d.",
        group, reply.signed_cc().process_id(), reply.signed_cc().signature().length(),
        reply.has_cc(),
        reply.cc().ccr());
    if (!reply.has_signed_cc()) {
      return;
    }

    if (!IsReplicaInGroup(reply.signed_cc().process_id(), group, config)) {
      Debug("[group %d] Phase1Reply from replica %lu who is not in group.",
          group, reply.signed_cc().process_id());
      return;
    }

    if (!verifier->Verify(keyManager->GetPublicKey(reply.signed_cc().process_id()),
          reply.signed_cc().data(), reply.signed_cc().signature())) {
      Debug("[group %i] Signature %s %s from replica %lu is not valid.", group,
            BytesToHex(reply.signed_cc().data(), 100).c_str(),
            BytesToHex(reply.signed_cc().signature(), 100).c_str(),
            reply.signed_cc().process_id());

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
      if (params.validateProofs) {
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
  if (params.validateProofs && params.signedMessages) {
    if (!reply.has_signed_p2_decision()) {
      Debug("[group %i] Phase2Reply missing signed_p2_decision.", group);
      return;
    }

    if (!IsReplicaInGroup(reply.signed_p2_decision().process_id(), group, config)) {
      Debug("[group %d] Phase2Reply from replica %lu who is not in group.",
          group, reply.signed_p2_decision().process_id());
      return;
    }

    if (!verifier->Verify(keyManager->GetPublicKey(
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

  if (params.validateProofs && params.signedMessages) {
    proto::Signature *sig = itr->second->p2ReplySigs.add_sigs();
    sig->set_process_id(reply.signed_p2_decision().process_id());
    *sig->mutable_signature()= reply.signed_p2_decision().signature();
  }

//TODO: Edit this to check for matching view too. Can be ommitted because correct client expects all in view 0?
//If it receives messages with view != 0 it needs to start its own fallback instance.
  if(params.validateProofs){
    if(!p2Decision->has_view()) return;
    if(p2Decision->view() != 0) return; //TODO: start fallback instance here. (case can happen if client is slow)
    //TODO: start fb "instance" and roll over this request to that fallback function. (makes it so that when
    //client receives bunch of messages from different views, they directly count towards the new Quorums needed)
  }

//Correct client KNOWS to expect only matching replies so we can just count those.
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

/////////////////////////////////////////FALLBACK CODE STARTS HERE ///////////////////////////////////////////

void ShardClient::HandlePhase1Relay(proto::RelayP1 &relayP1){

  Debug("RelayP1[%lu][%s].", relayP1.conflict_id(),
      TransactionDigest(relayP1.p1().txn(), params.hashDigest).c_str());
  uint64_t req_id = relayP1.conflict_id();
  auto itr = this->pendingPhase1s.find(req_id);
  if (itr == this->pendingPhase1s.end()) {
    return; // this is a stale request and no upcall is necessary!
  }
  itr->second->rcb(relayP1); //upcall to the registered relayP1 callback function.
}


void ShardClient::Phase1FB(proto::Phase1 &p1, const std::string &txnDigest, phase1FB_callbackA p1FBcbA,
  phase1FB_callbackB p1FBcbB, phase2FB_callback p2FBcb, writebackFB_callback wbFBcb, invokeFB_callback invFBcb) {
  Debug("[group %i] Sending PHASE1FB [%lu]", group, client_id);

  PendingFB* pendingFB = new PendingFB();
  pendingFallbacks[txnDigest] = pendingFB;

  PendingPhase1 *pendingPhase1 = new PendingPhase1(p1.req_id(), group, p1.txn(),
      txnDigest, config, keyManager, params, verifier);
  pendingFB->pendingP1 = pendingPhase1;

  //set all callbacks
  //TODO: need to have relayP1 of its own in theory, to support deeper deps.
  pendingFB->wbFBcb = wbFBcb;
  pendingFB->p1FBcbA = p1FBcbA;
  pendingFB->p1FBcbB = p1FBcbB;
  pendingFB->p2FBcb = p2FBcb;
  pendingFB->invFBcb = invFBcb;

  // create prepare request
  phase1FB.Clear();
  phase1FB.set_req_id(p1.req_id());
  *phase1FB.mutable_txn() = p1.txn();

  transport->SendMessageToGroup(this, group, phase1FB);

  pendingPhase1->requestTimeout->Reset();
}

//TODO: clean this up.
void ShardClient::HandlePhase1FBReply(proto::Phase1FBReply &p1fbr){ // update pendingFB state -- if complete, upcall to client


  std::string txnDigest = p1fbr.txn_digest();
  Debug("Handling P1FBReply [%s]", BytesToHex(txnDigest, 128).c_str());
  auto itr = this->pendingFallbacks.find(txnDigest);
  if (itr == this->pendingFallbacks.end()) {
    return; // this is a stale request
  }
  //TODO: check type:
  if(p1fbr.has_wb()){
    proto::Writeback wb = p1fbr.wb();
    itr->second->wbFBcb(wb);
    return;
  }

//Update views, since those might become necessary.
  UpdateViewStructure(txnDigest, p1fbr.attached_view());
  //If p2r :; Call HandlePhase2FB() --> this will invoke the Fallback. (this means that the view message must be passed also)
  if(p1fbr.has_p2r()){
    proto::Phase2Reply p2r = p1fbr.p2r();

    ProcessP2FBR(p2r, txnDigest);//, reply.attached_view());
    return;
  }
  if(p1fbr.has_p1r()){
      //If p1r :: //TODO: call existing HandlePingMessage

      //uint64_t reqID = p1fbr.req_id();
      proto::Phase1Reply reply = p1fbr.p1r();

      /// CODE PASTE:
          PendingPhase1 *pendingPhase1 = itr->second->pendingP1;

          bool hasSigned = (params.validateProofs && params.signedMessages) && (!reply.has_cc() || reply.cc().ccr() != proto::ConcurrencyControl::ABORT);

          const proto::ConcurrencyControl *cc = nullptr;
          //Check all validity criteria
          if (hasSigned) {
            Debug("[group %i] Verifying signed_cc because has_cc %d and ccr %d.", group, reply.has_cc(), reply.cc().ccr());
            if (!reply.has_signed_cc()) {
              return;
            }
            if (!IsReplicaInGroup(reply.signed_cc().process_id(), group, config)) {
              Debug("[group %d] Phase1FBReply from replica %lu who is not in group.",  group, reply.signed_cc().process_id());
              return;
            }
            if (!crypto::Verify(keyManager->GetPublicKey(reply.signed_cc().process_id()),
                  &reply.signed_cc().data()[0],
                  reply.signed_cc().data().length(),
                  &reply.signed_cc().signature()[0])) {
              Debug("[group %i] Signature %s from replica %lu is not valid.", group, BytesToHex(reply.signed_cc().signature(), 100).c_str(), reply.signed_cc().process_id());
              return;
            }
            if (!validatedCC.ParseFromString(reply.signed_cc().data())) {  //validatedCC is a global variable of type proto:CC
              return;
            }
            cc = &validatedCC;
          } else {
            UW_ASSERT(reply.has_cc());

            cc = &reply.cc();
          }
          Debug("[group %i] PHASE1FB callback ccr=%d", group, cc->ccr());
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
              Phase1FBDecision(itr->second);
              break;
            case FAST_ABORT:
              pendingPhase1->decision = proto::ABORT;
              pendingPhase1->fast = true;
              if (params.validateProofs) {
                pendingPhase1->conflict = cc->committed_conflict();
              }
              Phase1FBDecision(itr->second);
              break;
            case SLOW_COMMIT_FINAL:
              pendingPhase1->decision = proto::COMMIT;
              pendingPhase1->fast = false;
              Phase1FBDecision(itr->second);
              break;
            case SLOW_ABORT_FINAL:
              pendingPhase1->decision = proto::ABORT;
              pendingPhase1->fast = false;
              Phase1FBDecision(itr->second);
              break;
            case SLOW_COMMIT_TENTATIVE:

            //TODO:: need to edit this.
              if (!pendingPhase1->decisionTimeoutStarted) {
                pendingPhase1->decisionTimeout = new Timeout(transport,
                    phase1DecisionTimeout, [this, txnDigest]() {
                      auto itr = pendingFallbacks.find(txnDigest);
                      if (itr == pendingFallbacks.end()) {
                        return;
                      }
                      itr->second->pendingP1->decision = proto::COMMIT;
                      itr->second->pendingP1->fast = false;
                      Phase1FBDecision(itr->second);
                    }
                  );
                pendingPhase1->decisionTimeout->Reset();
                pendingPhase1->decisionTimeoutStarted = true;
              }
              break;

            case SLOW_ABORT_TENTATIVE:
              if (!pendingPhase1->decisionTimeoutStarted) {
                pendingPhase1->decisionTimeout = new Timeout(transport,
                    phase1DecisionTimeout, [this, txnDigest]() {
                      auto itr = pendingFallbacks.find(txnDigest);
                      if (itr == pendingFallbacks.end()) {
                        return;
                      }
                      itr->second->pendingP1->decision = proto::ABORT;
                      itr->second->pendingP1->fast = false;
                      Phase1FBDecision(itr->second);
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


}



  void ShardClient::Phase1FBDecision(PendingFB *pendingFB) {
    // itr->second->p1 = false;
    // PendingFB *pendingFB = itr->second;
    pendingFB->p1 = false;
    PendingPhase1 *pendingPhase1 = pendingFB->pendingP1;
    pendingFB->p1FBcbA(pendingPhase1->decision, pendingPhase1->fast, pendingPhase1->conflict, pendingPhase1->p1ReplySigs);
    //this->pendingFallbacks.erase(itr);
    delete pendingPhase1;  //p1 object in pendingFallback is obsolete.
  }

//version A) for p1 based Phase2.  grouped_sigs. //TODO:change callbacks.
  void ShardClient::Phase2FB(uint64_t id,
      const proto::Transaction &txn, const std::string &txnDigest,
      proto::CommitDecision decision,
      const proto::GroupedSignatures &groupedSigs) {

    Debug("[group %i] Sending PHASE2FB [%lu]", group, id);

    phase2FB.Clear();
    phase2FB.set_req_id(id);
    phase2FB.set_decision(decision);
    *phase2FB.mutable_txn_digest() = txnDigest;
    if (params.validateProofs && params.signedMessages) {
      *phase2FB.mutable_grouped_sigs() = groupedSigs;
    }
    transport->SendMessageToGroup(this, group, phase2FB);


  }
  //version B) for p2 based Phase2. p2_replies
//OVERLOAD Phase2FB so it has the 2 cases
  void ShardClient::Phase2FB(uint64_t id,
      const proto::Transaction &txn, const std::string &txnDigest,
      proto::CommitDecision decision,
      const proto::P2Replies &p2Replies) {

        return;
    Debug("[group %i] Sending PHASE2FB [%lu]", group, id);

    phase2FB.Clear();
    phase2FB.set_req_id(id);
    phase2FB.set_decision(decision);
    *phase2FB.mutable_txn_digest() = txnDigest;
    if (params.validateProofs && params.signedMessages) {
      *phase2FB.mutable_p2_replies() = p2Replies;
    }
    transport->SendMessageToGroup(this, group, phase2FB);


  }

void ShardClient::UpdateViewStructure(std::string txnDigest, const proto::AttachedView &ac){

  auto itr = this->pendingFallbacks.find(txnDigest);

  uint64_t current_view;
  bool update = false;
  uint64_t id;
  uint64_t set_view;

  if (params.validateProofs && params.signedMessages) {
        if(!ac.has_signed_current_view()) return;
        proto::SignedMessage signed_msg = ac.signed_current_view();
        proto::CurrentView new_view;
        new_view.ParseFromString(signed_msg.data());

        //only update data strucure if new view is bigger.
        if(itr->second->current_views.find(signed_msg.process_id()) != itr->second->current_views.end()){
          current_view =  itr->second->current_views[signed_msg.process_id()]->view;
          if(new_view.current_view() <= current_view) return;
        }
        // Check if replica ID in group.
        if(!IsReplicaInGroup(signed_msg.process_id(), group, config)) return;

        if(!crypto::Verify(keyManager->GetPublicKey(signed_msg.process_id()),
              &signed_msg.data()[0], signed_msg.data().length(),
              &signed_msg.signature()[0])) return;
        set_view = new_view.current_view();
        SignedView* s_view = new SignedView(set_view, signed_msg);
        update = true;
        itr->second->current_views[new_view.replica_id()] = s_view;
        id = signed_msg.process_id();

  }
  else{

    if(!ac.has_current_view()) return;
    proto::CurrentView new_view = ac.current_view();

    if(itr->second->current_views.find(new_view.replica_id()) != itr->second->current_views.end()){
        current_view =  itr->second->current_views[new_view.replica_id()]->view;
        if(new_view.current_view() <= current_view) return;
    }
    if(!IsReplicaInGroup(new_view.replica_id(), group, config)) return;
    uint64_t set_view = new_view.current_view();
    SignedView* s_view = new SignedView(set_view);
    update=true;
    itr->second->current_views[new_view.replica_id()] = s_view;
    id = new_view.replica_id();
  }

  if(update){

    itr->second->view_levels[current_view].erase(id);
    if(itr->second->view_levels[current_view].size() == 0){
      //itr->existing_levels.erase(current_view);
      itr->second->view_levels.erase(current_view);
    }

    if(itr->second->view_levels.find(set_view) == itr->second->view_levels.end()){  //if(itr->existing_levels.find(nv) == itr->existing_levels.end()){
      itr->second->view_levels[set_view] = std::set<uint64_t>();
    }
    itr->second->view_levels[set_view].insert(id);
    //itr->existing_levels.insert(new_view.view());

    //Dont do this here?

    // if(new_view.view() >= itr->second->max_view){
    //   ComputeMaxLevel(txnDigest);
    // }
    //ADD logic to call Invoke callback if it was missing new views to invoked
    if(itr->second->call_invokeFB){
      itr->second->view_invoker();
    }

  }
}

// void ShardClient::ComputeMaxLevel(std::string txnDigest){
//     auto itr = pendingFallbacks[txnDigest];
//     std::set<uint64_t> levels = itr->existing_levels;
//     std::set<int>::reverse_iterator rit;
//     uint64_t count = 0;
//
//     for (rit=levels.rbegin(); rit != levels.rend(); ++rit){
//       if(*rit < itr->max_view) return;
//       if(count + itr->view_levels[*rit].size() >= 3*config->f + 1){
//         itr->max_view = *rit + 1;
//         return;
//       }
//       elif(count+ itr->view_levels[*rit].size() >= config->f +1){
//         itr->max_view = *rit;
//         return;
//       }
//       count += itr->view_levels[*rit].size();
//
//     }
//   return;
// }

void ShardClient::ComputeMaxLevel(std::string txnDigest){
    auto itr = this->pendingFallbacks.find(txnDigest);

    std::map<uint64_t, std::set<uint64_t>>::reverse_iterator rit;
    uint64_t count = 0;

    for (rit=itr->second->view_levels.rbegin(); rit != itr->second->view_levels.rend(); ++rit){
      if(rit->first < itr->second->max_view) return;
      if(count + rit->second.size() >= 3*config->f + 1){
        itr->second->max_view = rit->first + 1;
        itr->second->catchup = false;
        return;
      }
      else if(count+ rit->second.size() >= config->f +1){
        itr->second->max_view = rit->first;
        itr->second->catchup = true;
        return;
      }
      count += rit->second.size();

    }
  return;
}

void ShardClient::HandlePhase2FBReply(proto::Phase2FBReply &p2fbr){

  std::string txnDigest = p2fbr.txn_digest();
  auto itr = this->pendingFallbacks.find(txnDigest);
  if (itr == this->pendingFallbacks.end()) {
    Debug("[group %i] Received stale Phase2FBReply for txn %s.", group, txnDigest.c_str());
    return; // this is a stale request
  }

//TODO: move this after message verification? to save processing cost if not necessary to compute views?
  UpdateViewStructure(txnDigest, p2fbr.attached_view());

  proto::Phase2Reply p2r = p2fbr.p2r();
  ProcessP2FBR(p2r, txnDigest); //, p2fbr.attached_view());

}


void ShardClient::ProcessP2FBR(proto::Phase2Reply &reply, std::string &txnDigest){ //, proto::AttachedView &view){
    auto itr = this->pendingFallbacks.find(txnDigest);
    const proto::Phase2Decision *p2Decision = nullptr;
    if (params.validateProofs && params.signedMessages) {
      if (!reply.has_signed_p2_decision()) {
        Debug("[group %i] Phase2FBReply missing signed_p2_decision.", group);
        return;
      }
      if (!IsReplicaInGroup(reply.signed_p2_decision().process_id(), group, config)) {
        Debug("[group %d] Phase2FBReply from replica %lu who is not in group.",
            group, reply.signed_p2_decision().process_id());
        return;
      }
      if (!crypto::Verify(keyManager->GetPublicKey(
              reply.signed_p2_decision().process_id()),
            &reply.signed_p2_decision().data()[0],
            reply.signed_p2_decision().data().length(),
            &reply.signed_p2_decision().signature()[0])) {
        return;
      }

      if (!validatedP2Decision.ParseFromString(reply.signed_p2_decision().data())) {
        return;
      }

      p2Decision = &validatedP2Decision;

    } else {
      p2Decision = &reply.p2_decision();
    }
    //if(!p2Decision->has_view()) return;
    proto::CommitDecision decision = p2Decision->decision();
    uint64_t view = p2Decision->view();
    uint64_t reqID = reply.req_id();

    Debug("[group %i] PHASE2FB reply with decision %d and view %lu", group,
        decision, view);

//that message is from likely obsolete views.
    if(itr->second->max_decision_view > view +1 ){
            return;
    }

    bool delete_old_views = false;
//create new entry for this view.
    if(itr->second->pendingP2s.find(view) == itr->second->pendingP2s.end()){
      PendingPhase2* pendingP2 = new PendingPhase2(reqID, decision);
      itr->second->pendingP2s[view] = pendingP2;
    }

//TODO: refactor the whole nition of Pending and Alt pending into a pair, thats part of the same map.

    // check matching decision for matching view

    if (decision == itr->second->pendingP2s[view]->decision) {
      if (params.validateProofs && params.signedMessages) {
        proto::Signature *sig = itr->second->pendingP2s[view]->p2ReplySigs.add_sigs();
        sig->set_process_id(reply.signed_p2_decision().process_id());
        *sig->mutable_signature()= reply.signed_p2_decision().signature();
      }
      itr->second->pendingP2s[view]->matchingReplies++;
      //check if we can update max_decision view and delete old ones. should only do this once f+1 received. Otherwise we let a byz delete our state.
      if(itr->second->pendingP2s[view]->matchingReplies > config->f){
        if(itr->second->max_decision_view < view){
              itr->second->max_decision_view = view;
              delete_old_views = true;
        }
      }
    }
//OTHERWISE ADD TO ALT SET.
    else{
      if(itr->second->ALTpendingP2s.find(view) == itr->second->ALTpendingP2s.end()){
        PendingPhase2* pendingP2 = new PendingPhase2(reqID, decision);
        itr->second->ALTpendingP2s[view] = pendingP2;
      }
      if (params.validateProofs && params.signedMessages) {
        proto::Signature *sig = itr->second->ALTpendingP2s[view]->p2ReplySigs.add_sigs();
        sig->set_process_id(reply.signed_p2_decision().process_id());
        *sig->mutable_signature()= reply.signed_p2_decision().signature();
      }
      itr->second->ALTpendingP2s[view]->matchingReplies++;
      if(itr->second->ALTpendingP2s[view]->matchingReplies > config->f){
        if(itr->second->max_decision_view < view){
              itr->second->max_decision_view = view;
              delete_old_views = true;
        }
      }
    }
    //Can return directly to writeback
    if (itr->second->pendingP2s[view]->matchingReplies >= QuorumSize(config)) {
      PendingFB *pendingFB = itr->second;
      pendingFB->p2FBcb(pendingFB->pendingP2s[view]->decision , pendingFB->pendingP2s[view]->p2ReplySigs);
      this->pendingFallbacks.erase(itr);
      delete pendingFB;
    }
    else if (itr->second->ALTpendingP2s[view]->matchingReplies >= QuorumSize(config)) {
      PendingFB *pendingFB = itr->second;
        pendingFB->p2FBcb(pendingFB->ALTpendingP2s[view]->decision , pendingFB->ALTpendingP2s[view]->p2ReplySigs);
        //TODO:Edit this callback so that it includes view too. Then modify both client and server code to check proofs for matching view as well.
      this->pendingFallbacks.erase(itr);
      delete pendingFB;
    }


    if(delete_old_views){
              //delete all entries for views < max_view -1. They are pretty much obsolete.
                std::map<uint64_t, PendingPhase2*>::iterator it;
                for (it=itr->second->pendingP2s.begin(); it != itr->second->pendingP2s.end(); it++){
                   if(it->first >= itr->second->max_decision_view -1){
                     break;
                   }
                   else{
                        PendingPhase2 *deleteP2 = itr->second->pendingP2s[it->first];
                        itr->second->pendingP2s.erase(it->first);
                        delete deleteP2;
                   }
                }
                for (it=itr->second->ALTpendingP2s.begin(); it != itr->second->ALTpendingP2s.end(); it++){
                   if(it->first >= itr->second->max_decision_view -1){
                     break;
                   }
                   else{
                     PendingPhase2 *deleteP2 = itr->second->ALTpendingP2s[it->first];
                     itr->second->ALTpendingP2s.erase(it->first);
                     delete deleteP2;
                   }
                }
      }

    //Otherwise, check if we are still doing p1 simultaneously

    if(itr->second->p1){
      uint64_t id = reply.signed_p2_decision().process_id();
      if(itr->second->process_ids.find(id) == itr->second->process_ids.end()){
        itr->second->process_ids.insert(id);
        proto::Phase2Reply *new_item  = itr->second->p2Replies[decision]->add_p2replies();
        *new_item = reply;
      }
      proto::P2Replies* p2Replies = itr->second->p2Replies[decision];
      if(p2Replies->p2replies().size() == config->f +1 ){
        itr->second->p1 = false;
        PendingFB *pendingFB = itr->second;
        pendingFB->p1FBcbB(decision, *pendingFB->p2Replies[decision]);
      }
    }



//max decision view represents f+1 replicas. Implies that this is the current view.
//CALL Fallback if detected divergence for newest accepted view. (calling it for older ones is useless)
    if(itr->second->max_decision_view == view
      && itr->second->pendingP2s[view]->matchingReplies >= config->f +1
      && itr->second->ALTpendingP2s[view]->matchingReplies >= config->f +1){
        itr->second->invFBcb();
      }
}

void ShardClient::InvokeFB(uint64_t conflict_id, std::string txnDigest, proto::Transaction &txn, proto::CommitDecision decision, proto::P2Replies &p2Replies){
  ComputeMaxLevel(txnDigest);
  auto itr = this->pendingFallbacks.find(txnDigest);
  if(itr == this->pendingFallbacks.end()) return;

  if(itr->second->max_view <= itr->second->last_view){
    itr->second->call_invokeFB = true;
    itr->second->view_invoker = std::bind(&ShardClient::InvokeFB, this, conflict_id, txnDigest, txn, decision, p2Replies);
  }
  else{
    itr->second->call_invokeFB = false;
    itr->second->last_view = itr->second->max_view;


      proto::SignedMessages view_signed;
      std::map<uint64_t, std::set<uint64_t>>::reverse_iterator rit;
      uint64_t count;

      if(itr->second->catchup){
        count = config->f+1;
      }
      else{
        count = 3*config->f +1;
      }
      for (rit=itr->second->view_levels.rbegin(); rit != itr->second->view_levels.rend(); ++rit){

        for(auto id: rit->second){
          SignedView* sv = itr->second->current_views[id];
          proto::SignedMessage* sm = view_signed.add_sig_msgs();
          *sm = sv->signed_view;
          count++;
          if(count == 0) break;
        }
        if(count == 0) break;
      }

      phase2FB.Clear();
      phase2FB.set_req_id(conflict_id);
      phase2FB.set_decision(decision);
      phase2FB.set_txn_digest(txnDigest);
      *phase2FB.mutable_txn() = txn;
      *phase2FB.mutable_p2_replies() = p2Replies;

      invokeFB.Clear();
      invokeFB.set_req_id(conflict_id);
      invokeFB.set_txn_digest(txnDigest);
      *invokeFB.mutable_p2fb() = phase2FB;
      invokeFB.set_proposed_view(itr->second->max_view);
      *invokeFB.mutable_view_signed() = view_signed;

      transport->SendMessageToGroup(this, group, invokeFB);
      Debug("[group %i] Sent InvokeFB[%lu]", group, client_id);
  }

}

void ShardClient::WritebackFB_fast(std::string txnDigest, proto::Writeback &wb) {

  transport->SendMessageToGroup(this, group, wb);
  Debug("[group %i] Sent FB-WRITEBACK[%lu]", group, client_id);

  // Delete PendingFB instance.
  auto itr = pendingFallbacks.find(txnDigest);
  if(itr != pendingFallbacks.end()){
    PendingFB *pendFB = itr->second;
    pendingFallbacks.erase(txnDigest);
    delete pendFB;
  }

}


} // namespace indicus
