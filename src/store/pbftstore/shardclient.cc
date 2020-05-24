#include "store/pbftstore/shardclient.h"
#include "store/pbftstore/common.h"

namespace pbftstore {

ShardClient::ShardClient(const transport::Configuration& config, Transport *transport,
    uint64_t group_idx,
    bool signMessages, bool validateProofs,
    KeyManager *keyManager, Stats* stats) :
    config(config), transport(transport),
    group_idx(group_idx),
    signMessages(signMessages), validateProofs(validateProofs),
    keyManager(keyManager), stats(stats) {
  transport->Register(this, config, -1, -1);
  readReq = 0;
}

ShardClient::~ShardClient() {}

bool ShardClient::validateReadProof(const proto::CommitProof& commitProof, const std::string& key,
  const std::string& value, const Timestamp& timestamp) {
    // hack for load:
    if (timestamp.getID() == 0 && timestamp.getTimestamp() == 0) {
      Debug("Using preloaded key");
      return true;
    }

    // First, verify the transaction
    Debug("Validating read proof");

    // txn must have timestamp of write
    if (Timestamp(commitProof.txn().timestamp()) != timestamp) {
      return false;
    }
    Debug("timestamp valid");

    bool found_write = false;

    for (const auto& write : commitProof.txn().writeset()) {
      if (write.key() == key && write.value() == value) {
        found_write = true;
        break;
      }
    }

    if (!found_write) {
      return false;
    }
    Debug("write valid");

    // Verified Transaction at this point

    // Next, verify that the decision is valid for the transaction

    std::string proofTxnDigest = TransactionDigest(commitProof.txn());

    // make sure the writeback message is for the transaction
    if (commitProof.writeback_message().txn_digest() != proofTxnDigest) {
      return false;
    }
    Debug("commit digest valid");

    if (commitProof.writeback_message().status() != REPLY_OK) {
      return false;
    }
    Debug("writeback status valid");

    if (!verifyGDecision(commitProof.writeback_message(), commitProof.txn(), keyManager, signMessages, config.f)) {
      return false;
    }
    Debug("proof valid");

    return true;
  }

void ShardClient::ReceiveMessage(const TransportAddress &remote,
    const std::string &t, const std::string &d,
    void *meta_data) {
      Debug("handling message of type %s", t.c_str());
  proto::SignedMessage signedMessage;
  std::string type;
  std::string data;

  bool recvSignedMessage = false;
  if (t == signedMessage.GetTypeName()) {
    if (!signedMessage.ParseFromString(d)) {
      return;
    }

    if (!ValidateSignedMessage(signedMessage, keyManager, data, type)) {
      Debug("signature was invalid");
      return;
    }
    recvSignedMessage = true;
    Debug("signature was valid");
  } else {
    type = t;
    data = d;
  }

  proto::ReadReply readReply;
  proto::TransactionDecision transactionDecision;
  proto::GroupedDecisionAck groupedDecisionAck;
  if (type == readReply.GetTypeName()) {
    readReply.ParseFromString(data);

    if (signMessages && !recvSignedMessage) {
      return;
    }

    HandleReadReply(readReply, signedMessage);
  } else if (type == transactionDecision.GetTypeName()) {
    transactionDecision.ParseFromString(data);

    if (signMessages && !recvSignedMessage) {
      return;
    }

    HandleTransactionDecision(transactionDecision, signedMessage);
  } else if (type == groupedDecisionAck.GetTypeName()) {
    groupedDecisionAck.ParseFromString(data);

    if (signMessages && !recvSignedMessage) {
      return;
    }

    HandleWritebackReply(groupedDecisionAck, signedMessage);
  }
}

// ================================
// ======= MESSAGE HANDLERS =======
// ================================

void ShardClient::HandleReadReply(const proto::ReadReply& readReply, const proto::SignedMessage& signedMsg) {
  Debug("Handling a read reply");

  // get the read request id from the reply
  uint64_t reqId = readReply.req_id();
  Debug("Read red id: %lu", reqId);

  // try and find a matching pending read based on the request
  if (pendingReads.find(reqId) != pendingReads.end()) {
    PendingRead* pendingRead = &pendingReads[reqId];
    // we always mark a reply even if it fails because the read could fail
    if (signMessages) {
      uint64_t replica_id = signedMsg.replica_id();
      // make sure the replica is from this shard
      if (replica_id / config.n != (uint64_t) group_idx) {
        Debug("Read Reply: replica not in group");
        return;
      }
      // insert the signed replica id as a received reply
      pendingRead->receivedReplies.insert(replica_id);
    } else {
      // insert a new fake id into the received replies
      pendingRead->receivedReplies.insert(pendingRead->receivedReplies.size());
    }
    if (readReply.status() == REPLY_OK) {
      Timestamp rts(readReply.value_timestamp());
      if (validateProofs) {
        // if the proof is invalid, stop processing this reply
        if (!validateReadProof(readReply.commit_proof(), readReply.key(), readReply.value(), rts)) {
          return;
        }
      }
      // if we haven't recorded a read result yet or we have a higher read,
      // make this reply the new max
      if (pendingRead->status == REPLY_FAIL || rts > pendingRead->maxTs) {
        Debug("Updating max read reply");
        pendingRead->maxTs = rts;
        pendingRead->maxValue = readReply.value();
        pendingRead->maxCommitProof = readReply.commit_proof();
        pendingRead->status = REPLY_OK;
      }
    }

    Debug("reply size: %lu", pendingRead->receivedReplies.size());
    if (pendingRead->receivedReplies.size() >= pendingRead->numResultsRequired) {
      if (pendingRead->timeout != nullptr) {
        pendingRead->timeout->Stop();
      }
      read_callback rcb = pendingRead->rcb;
      std::string value = pendingRead->maxValue;
      Timestamp readts = pendingRead->maxTs;
      std::string key = readReply.key();
      uint64_t status = pendingRead->status;
      pendingReads.erase(reqId);
      rcb(status, key, value, readts);
    }
  }
}


void ShardClient::HandleTransactionDecision(const proto::TransactionDecision& transactionDecision, const proto::SignedMessage& signedMsg) {
  Debug("Handling transaction decision");

  std::string digest = transactionDecision.txn_digest();
  DebugHash(digest);
  // only handle decisions for my shard
  // NOTE: makes the assumption that numshards == numgroups
  if (transactionDecision.shard_id() == (uint64_t) group_idx) {
    if (signMessages) {
      stats->Increment("handle_tx_dec_s",1);
      // Debug("signed packed msg: %s", string_to_hex(signedMsg.packed_msg()).c_str());
      // get the pending signed preprepare
      if (pendingSignedPrepares.find(digest) != pendingSignedPrepares.end()) {
        Debug("Adding signed id to a set: %lu", signedMsg.replica_id());

        PendingSignedPrepare* psp = &pendingSignedPrepares[digest];
        uint64_t add_id = signedMsg.replica_id();
        // make sure this id is actually in the group
        if (add_id / config.n == (uint64_t) group_idx) {
          if (transactionDecision.status() == REPLY_OK) {
            // add the decision to the list as proof
            psp->receivedValidSigs[add_id] = signedMsg.signature();
            // Debug("signature for %lu: %s", add_id, string_to_hex(signedMsg.signature()).c_str());
          } else {
            psp->receivedFailedIds.insert(add_id);
          }
        }

        proto::GroupedSignedMessage groupSignedMsg;

        // once we have enough valid requests, construct the grouped decision
        // and return success
        if (psp->receivedValidSigs.size() >= (uint64_t) config.f + 1) {
          Debug("Got enough transaction decisions, executing callback");
          // set the packed decision
          groupSignedMsg.set_packed_msg(psp->validDecisionPacked);
          // Debug("packed decision: %s", string_to_hex(psp->validDecisionPacked).c_str());

          // add the signatures
          for (const auto& pair : psp->receivedValidSigs) {
            (*groupSignedMsg.mutable_signatures())[pair.first] = pair.second;
          }

          // invoke the callback with the signed grouped decision
          signed_prepare_callback pcb = psp->pcb;
          if (psp->timeout != nullptr) {
            psp->timeout->Stop();
          }
          pendingSignedPrepares.erase(digest);
          pcb(REPLY_OK, groupSignedMsg);
          return;
        }
        // if we get f+1 failures, we can return early
        if (psp->receivedFailedIds.size() >= (uint64_t) config.f + 1) {
          Debug("Not enough valid txn decisions, failing");
          signed_prepare_callback pcb = psp->pcb;
          if (psp->timeout != nullptr) {
            psp->timeout->Stop();
          }
          pendingSignedPrepares.erase(digest);
          // adding sigs to the grouped signed msg would be worthless
          pcb(REPLY_FAIL, groupSignedMsg);
          return;
        }
      }
    } else {
      stats->Increment("handle_tx_dec",1);
      if (pendingPrepares.find(digest) != pendingPrepares.end()) {
        PendingPrepare* pp = &pendingPrepares[digest];
        if (transactionDecision.status() == REPLY_OK) {
          uint64_t add_id = pp->receivedOkIds.size();
          // add the decision to the list as proof
          pp->receivedOkIds.insert(add_id);
        } else {
          // Kinda jank but just don't use these ids
          uint64_t add_id = pp->receivedFailedIds.size();
          pp->receivedFailedIds.insert(add_id);
        }

        // once we have enough valid requests, construct the grouped decision
        // and return success
        if (pp->receivedOkIds.size() >= (uint64_t) config.f + 1) {
          proto::TransactionDecision validDecision = pp->validDecision;
          // invoke the callback if we have enough of the same decision
          prepare_callback pcb = pp->pcb;
          if (pp->timeout != nullptr) {
            pp->timeout->Stop();
          }
          pendingPrepares.erase(digest);
          pcb(REPLY_OK, validDecision);
          return;
        }
        // f+1 failures mean that we will always return fail
        if (pp->receivedFailedIds.size() >= (uint64_t) config.f + 1) {
          proto::TransactionDecision failedDecision;
          failedDecision.set_status(REPLY_FAIL);
          prepare_callback pcb = pendingPrepares[digest].pcb;
          if (pp->timeout != nullptr) {
            pp->timeout->Stop();
          }
          pendingPrepares.erase(digest);
          pcb(REPLY_FAIL, failedDecision);
          return;
        }
      }
    }
  } else {
    stats->Increment("wrong_dec_shard",1);
  }
}

void ShardClient::HandleWritebackReply(const proto::GroupedDecisionAck& groupedDecisionAck, const proto::SignedMessage& signedMsg) {
  Debug("Handling Writeback reply");

  std::string digest = groupedDecisionAck.txn_digest();
  DebugHash(digest);
  if (pendingWritebacks.find(digest) != pendingWritebacks.end()) {
    PendingWritebackReply* pw = &pendingWritebacks[digest];

    uint64_t replica_id = pw->receivedAcks.size() + pw->receivedFails.size();
    if (signMessages) {
      replica_id = signedMsg.replica_id();
    }

    if (groupedDecisionAck.status() == REPLY_OK) {
      Debug("got a decision ack from %lu", replica_id);
      pw->receivedAcks.insert(replica_id);
    } else {
      Debug("got a decision failure from %lu", replica_id);
      pw->receivedFails.insert(replica_id);
    }

    // 2f+1 because we want a quorum of honest users to acknowledge (for fault tolerance)
    if (pw->receivedAcks.size() >= (uint64_t) 2*config.f + 1) {
      Debug("Got enough writeback acks");
      // if the list has enough replicas, we can invoke the callback
      writeback_callback wcb = pendingWritebacks[digest].wcb;
      if (pendingWritebacks[digest].timeout != nullptr) {
        pendingWritebacks[digest].timeout->Stop();
      }
      pendingWritebacks.erase(digest);
      wcb(REPLY_OK);
      return;
    }

    // once we get f + 1 fails, impossible to get 2f+1 succeeds
    if (pw->receivedFails.size() >= (uint64_t) config.f + 1) {
      Debug("Unable to get enough writeback acks, failing");
      writeback_callback wcb = pendingWritebacks[digest].wcb;
      if (pendingWritebacks[digest].timeout != nullptr) {
        pendingWritebacks[digest].timeout->Stop();
      }
      pendingWritebacks.erase(digest);
      wcb(REPLY_FAIL);
    }
  }
}

// ================================
// ==== SHARD CLIENT INTERFACE ====
// ================================

// Get the value corresponding to key.
void ShardClient::Get(const std::string &key, const Timestamp &ts,
    uint64_t numResults, read_callback gcb, read_timeout_callback gtcb,
    uint32_t timeout) {
  Debug("Client get for %s", key.c_str());

  proto::Read read;
  uint64_t reqId = readReq++;
  Debug("Get id: %lu", reqId);
  read.set_req_id(reqId);
  read.set_key(key);
  ts.serialize(read.mutable_timestamp());

  transport->SendMessageToGroup(this, group_idx, read);
  PendingRead pr;
  pr.rcb = gcb;
  pr.numResultsRequired = numResults;
  pr.status = REPLY_FAIL;
  // every ts should be bigger than this one
  pr.maxTs = Timestamp();
  pr.timeout = new Timeout(transport, timeout, [this, reqId, gtcb]() {
    Debug("Get timeout called (but nothing was done)");
      stats->Increment("g_tout", 1);
      fprintf(stderr,"g_tout recv %lu\n",  this->pendingReads[reqId].numResultsRequired);
      for (const auto& recv : this->pendingReads[reqId].receivedReplies) {
        fprintf(stderr,"%lu\n", recv);
      }
    // this->pendingReads.erase(reqId);
    // gtcb(reqId, key);
  });
  pr.timeout->Start();
  // pr.timeout = nullptr;

  pendingReads[reqId] = pr;

}

std::string ShardClient::CreateValidPackedDecision(std::string digest) {
  proto::TransactionDecision validDecision;
  validDecision.set_status(REPLY_OK);
  validDecision.set_txn_digest(digest);
  validDecision.set_shard_id(group_idx);

  proto::PackedMessage packedDecision;
  packedDecision.set_type(validDecision.GetTypeName());
  packedDecision.set_msg(validDecision.SerializeAsString());

  return packedDecision.SerializeAsString();
}

// send a request with this as the packed message
void ShardClient::Prepare(const proto::Transaction& txn, prepare_callback pcb,
    prepare_timeout_callback ptcb, uint32_t timeout) {
  Debug("Handling client prepare");

  std::string digest = TransactionDigest(txn);
  if (pendingPrepares.find(digest) == pendingPrepares.end()) {
    stats->Increment("shard_prepare",1);
    proto::Request request;
    DebugHash(digest);
    request.set_digest(digest);
    request.mutable_packed_msg()->set_msg(txn.SerializeAsString());
    request.mutable_packed_msg()->set_type(txn.GetTypeName());

    Debug("Sending txn to all replicas in shard");
    transport->SendMessageToGroup(this, group_idx, request);

    PendingPrepare pp;
    pp.pcb = pcb;
    proto::TransactionDecision validDecision;
    validDecision.set_status(REPLY_OK);
    validDecision.set_txn_digest(digest);
    validDecision.set_shard_id(group_idx);
    // this is what the tx decisions should look like for valid replies
    pp.validDecision = validDecision;
    pp.timeout = new Timeout(transport, timeout, [this, digest, ptcb]() {
      Debug("Prepare timeout called (but nothing was done)");
      stats->Increment("p_tout", 1);
      fprintf(stderr,"p_tout recv %d\n", group_idx);
      fprintf(stderr,"ack\n");
      for (const auto& recv : this->pendingPrepares[digest].receivedOkIds) {
        fprintf(stderr,"%lu\n", recv);
      }
      fprintf(stderr,"nak:\n");
      for (const auto& recv : this->pendingPrepares[digest].receivedFailedIds) {
        fprintf(stderr,"%lu\n", recv);
      }
      // this->pendingPrepares.erase(digest);
      // ptcb(REPLY_FAIL);
    });
    pp.timeout->Start();
    // pp.timeout = nullptr;

    pendingPrepares[digest] = pp;

  } else {
    Debug("prepare called on already prepared tx");
  }
}

void ShardClient::SignedPrepare(const proto::Transaction& txn, signed_prepare_callback pcb,
    prepare_timeout_callback ptcb, uint32_t timeout) {
  Debug("Handling client signed prepare");
  std::string digest = TransactionDigest(txn);
  if (pendingSignedPrepares.find(digest) == pendingSignedPrepares.end()) {
    proto::Request request;
    request.set_digest(digest);
    request.mutable_packed_msg()->set_msg(txn.SerializeAsString());
    request.mutable_packed_msg()->set_type(txn.GetTypeName());
    stats->Increment("shard_prepare_s",1);

    Debug("Sending txn to all replicas in shard");
    transport->SendMessageToGroup(this, group_idx, request);

    PendingSignedPrepare psp;
    psp.pcb = pcb;
    // this is what the tx decisions should look like for valid replies
    psp.validDecisionPacked = CreateValidPackedDecision(digest);
    psp.timeout = new Timeout(transport, timeout, [this, digest, ptcb]() {
      Debug("Prepare signed timeout called (but nothing was done)");
      stats->Increment("ps_tout", 1);
      fprintf(stderr,"ps_tout recv %d\n", group_idx);
      fprintf(stderr,"ack\n");
      for (const auto& recv : this->pendingSignedPrepares[digest].receivedValidSigs) {
        fprintf(stderr,"%lu\n", recv.first);
      }
      fprintf(stderr,"nak:\n");
      for (const auto& recv : this->pendingSignedPrepares[digest].receivedFailedIds) {
        fprintf(stderr,"%lu\n", recv);
      }
      // this->pendingSignedPrepares.erase(digest);
      // ptcb(REPLY_FAIL);
    });
    psp.timeout->Start();
    // psp.timeout = nullptr;

    pendingSignedPrepares[digest] = psp;

  } else {
    Debug("prepare signed called on already prepared tx");
  }
}

void ShardClient::Commit(const std::string& txn_digest, const proto::ShardDecisions& dec,
    writeback_callback wcb, writeback_timeout_callback wtcp, uint32_t timeout) {
  Debug("Handling client commit");
  if (pendingWritebacks.find(txn_digest) == pendingWritebacks.end()) {
    proto::GroupedDecision groupedDecision;
    groupedDecision.set_status(REPLY_OK);
    groupedDecision.set_txn_digest(txn_digest);
    *groupedDecision.mutable_decisions() = dec;
    stats->Increment("shard_commit", 1);

    Debug("Sending commit to all replicas in shard");
    transport->SendMessageToGroup(this, group_idx, groupedDecision);

    PendingWritebackReply pwr;
    pwr.wcb = wcb;
    pwr.timeout = new Timeout(transport, timeout, [this, txn_digest, wtcp]() {
      Debug("Writeback timeout called (but nothing was done)");
      stats->Increment("c_tout", 1);
      fprintf(stderr,"c_tout recv %d\n", group_idx);
      fprintf(stderr, "txn: %s\n", txn_digest.c_str());
      fprintf(stderr,"ack\n");
      for (const auto& recv : this->pendingWritebacks[txn_digest].receivedAcks) {
        fprintf(stderr,"%lu\n", recv);
      }
      fprintf(stderr,"nak:\n");
      for (const auto& recv : this->pendingWritebacks[txn_digest].receivedFails) {
        fprintf(stderr,"%lu\n", recv);
      }

      // this->pendingWritebacks.erase(digest);
      // wtcp(REPLY_FAIL);
    });
    pwr.timeout->Start();
    // pwr.timeout = nullptr;

    pendingWritebacks[txn_digest] = pwr;

  } else {
    Debug("commit called on already committed tx");
  }
}

void ShardClient::CommitSigned(const std::string& txn_digest, const proto::ShardSignedDecisions& dec,
    writeback_callback wcb, writeback_timeout_callback wtcp, uint32_t timeout) {
  Debug("Handling client commit signed");
  if (pendingWritebacks.find(txn_digest) == pendingWritebacks.end()) {
    proto::GroupedDecision groupedDecision;
    groupedDecision.set_status(REPLY_OK);
    groupedDecision.set_txn_digest(txn_digest);
    *groupedDecision.mutable_signed_decisions() = dec;
    stats->Increment("shard_commit_s", 1);

    Debug("Sending commit to all replicas in shard");
    transport->SendMessageToGroup(this, group_idx, groupedDecision);

    PendingWritebackReply pwr;
    pwr.wcb = wcb;
    pwr.timeout = new Timeout(transport, timeout, [this, txn_digest, wtcp]() {
      Debug("Writeback signed timeout called (but nothing was done)");
      stats->Increment("cs_tout", 1);
      fprintf(stderr,"cs_tout recv %d\n", group_idx);
      fprintf(stderr,"ack\n");
      for (const auto& recv : this->pendingWritebacks[txn_digest].receivedAcks) {
        fprintf(stderr,"%lu\n", recv);
      }
      fprintf(stderr,"nak:\n");
      for (const auto& recv : this->pendingWritebacks[txn_digest].receivedFails) {
        fprintf(stderr,"%lu\n", recv);
      }
      // this->pendingWritebacks.erase(digest);
      // wtcp(REPLY_FAIL);
    });
    pwr.timeout->Start();
    // pwr.timeout = nullptr;

    pendingWritebacks[txn_digest] = pwr;

    // TODO timeout
  } else {
    Debug("commit signed called on already committed tx");
  }
}

void ShardClient::Abort(std::string txn_digest) {
  Debug("Handling client abort");
  // TODO should techincally include a proof
  if (pendingWritebacks.find(txn_digest) == pendingWritebacks.end()) {
    proto::GroupedDecision groupedDecision;
    groupedDecision.set_status(REPLY_FAIL);
    groupedDecision.set_txn_digest(txn_digest);
    proto::ShardDecisions sd;
    *groupedDecision.mutable_decisions() = sd;

    proto::Request request;
    request.set_digest(crypto::Hash(groupedDecision.SerializeAsString()));
    request.mutable_packed_msg()->set_msg(groupedDecision.SerializeAsString());
    request.mutable_packed_msg()->set_type(groupedDecision.GetTypeName());

    stats->Increment("shard_abort", 1);
    Debug("AB abort to all replicas in shard");
    transport->SendMessageToGroup(this, group_idx, request);
  } else {
    Debug("abort called on already aborted tx");
  }
}

}
