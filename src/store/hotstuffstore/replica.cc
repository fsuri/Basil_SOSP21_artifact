#include "store/hotstuffstore/replica.h"
#include "store/hotstuffstore/common.h"

namespace hotstuffstore {

using namespace std;

// No view change, but still view numbers
// Client -> leader request: (op, ts, client address)
// Leader -> All - leader preprepare: (view, seq, d(m))_i, m=(op, ts, client
// address) All - leader -> All prepare: (view, seq, d(m))_i once 1 preprepare
// and 2f prepares for (view, seq d(m)) then All -> All commit: (view, seq,
// d(m))_i

// Faults
// Primary ignores client request
// client sends request to all, replicas send to primary, start timeout waiting
// for preprepare Primary doesn't send preprepare to all, if some client gets
// prepare for request it doesn't have preprepare, start timeout Primary ignores
// f correct replicas, sends preprepare to f+1, colludes with f incorrect, f
// correct can't remove primary, so whenever you receive a preprepare you need
// to start a timer until you receive 2f prepares multicast commit, still need
// to start timeout
//      - primary sends prepare to f+1 correct, f wrong, f correct ignored (f
//      ignored will send view change messages, not enough)
//      - f+1 correct get 2f prepares (from each other and f wrong)
//      - f+1 send commit to all, should the f correct replicas accept the
//      commit messages

Replica::Replica(const transport::Configuration &config, KeyManager *keyManager,
  App *app, int groupIdx, int idx, bool signMessages, uint64_t maxBatchSize,
  uint64_t batchTimeoutMS, bool primaryCoordinator, bool requestTx, Transport *transport)
    : config(config),
      // HotStuff
      hotstuff_interface(groupIdx, idx),
      keyManager(keyManager), app(app), groupIdx(groupIdx), idx(idx),
    id(groupIdx * config.n + idx), signMessages(signMessages), maxBatchSize(maxBatchSize),
    batchTimeoutMS(batchTimeoutMS), primaryCoordinator(primaryCoordinator), requestTx(requestTx), transport(transport) {
  transport->Register(this, config, groupIdx, idx);

  // intial view
  currentView = 0;
  // initial seqnum
  nextSeqNum = 0;
  execSeqNum = 0;
  execBatchNum = 0;

  batchTimerRunning = false;
  nextBatchNum = 0;

  Debug("Initialized replica at %d %d", groupIdx, idx);

  stats = app->mutableStats();


  // assume these are somehow secretly shared before hand
  for (uint64_t i = 0; i < config.n; i++) {
    if (i > idx) {
      sessionKeys[i] = std::string(8, (char) idx + 0x30) + std::string(8, (char) i + 0x30);
    } else {
      sessionKeys[i] = std::string(8, (char) i + 0x30) + std::string(8, (char) idx + 0x30);
    }
  }
}

Replica::~Replica() {}

bool Replica::ValidateHMACedMessage(const proto::SignedMessage &signedMessage, std::string &data, std::string &type) {
  proto::PackedMessage packedMessage;
  packedMessage.ParseFromString(signedMessage.packed_msg());
  data = packedMessage.msg();
  type = packedMessage.type();

  proto::HMACs hmacs;
  hmacs.ParseFromString(signedMessage.signature());
  return crypto::verifyHMAC(signedMessage.packed_msg(), (*hmacs.mutable_hmacs())[idx], sessionKeys[signedMessage.replica_id() % config.n]);
}

void Replica::CreateHMACedMessage(const ::google::protobuf::Message &msg, proto::SignedMessage& signedMessage) {
  proto::PackedMessage packedMsg;
  *packedMsg.mutable_msg() = msg.SerializeAsString();
  *packedMsg.mutable_type() = msg.GetTypeName();
  // TODO this is not portable. SerializeAsString may not return the same
  // result every time
  std::string msgData = packedMsg.SerializeAsString();
  signedMessage.set_packed_msg(msgData);
  signedMessage.set_replica_id(id);

  proto::HMACs hmacs;
  for (uint64_t i = 0; i < config.n; i++) {
    (*hmacs.mutable_hmacs())[i] = crypto::HMAC(msgData, sessionKeys[i]);
  }
  signedMessage.set_signature(hmacs.SerializeAsString());
}

void Replica::ReceiveMessage(const TransportAddress &remote, const string &t,
                          const string &d, void *meta_data) {
    string type;
    string data;
    bool recvSignedMessage = false;

    Debug("Received message of type %s", t.c_str());

    // This may be obsolete because it is for PBFT
    if (t == tmpsignedMessage.GetTypeName()) {
        if (!tmpsignedMessage.ParseFromString(d)) {
            return;
        }

        if (!ValidateHMACedMessage(tmpsignedMessage, data, type)) {
            Debug("Message is invalid!");
            stats->Increment("invalid_sig",1);
            return;
        }
        recvSignedMessage = true;
        Debug("Message is valid!");
        Debug("Message is from %lu", tmpsignedMessage.replica_id());
    } else {
        type = t;
        data = d;
    }


    if (type == recvgrouped.GetTypeName() ||
        type == recvbatchedRequest.GetTypeName() ||
        type == recvab.GetTypeName() ||
        type == recvrr.GetTypeName() ||
        type == recvpreprepare.GetTypeName() ||
        type == recvprepare.GetTypeName() ||
        type == recvcommit.GetTypeName()) {
        // old message types for PBFT
        Panic("unimplemented: PBFT message received by replica");
    }

    if (type == recvrequest.GetTypeName()) {
        // Prepare message from shardclient
        recvrequest.ParseFromString(data);
        HandleRequest(remote, recvrequest);
    } else if (true) {
        // Other requests from shardclient (Read)
        Notice("Sending request to app");
        ::google::protobuf::Message* reply = app->HandleMessage(type, data);
        if (reply != nullptr) {
            transport->SendMessage(this, remote, *reply);
            delete reply;
        } else {
            Debug("Invalid request of type %s", type.c_str());
        }
    }
}

void Replica::HandleRequest(const TransportAddress &remote,
                               const proto::Request &request) {
  Notice("Handling request message");

  string digest = request.digest();

  if (requests.find(digest) == requests.end()) {
      Notice("new request: %s with digest %d bytes", request.packed_msg().type().c_str(), digest.length());

    requests[digest] = request.packed_msg();
    // clone remote mapped to request for reply
    replyAddrs[digest] = remote.clone();

    // prepare the callback function for HotStuff
    hotstuff_exec_callback execb = [this](const std::string &digest) {
        if (requests.find(digest) != requests.end()) {
            proto::PackedMessage packedMsg = requests[digest];
            std::vector<::google::protobuf::Message*> replies = app->Execute(packedMsg.type(), packedMsg.msg());
            for (const auto& reply : replies) {
                if (reply != nullptr) {
                    Debug("Sending reply");
                    stats->Increment("execs_sent",1);
                    transport->SendMessage(this, *replyAddrs[digest], *reply);
                    delete reply;
                } else {
                    Debug("Invalid execution");
                }
            }
        } else {
            Panic("unimplemented: try to execute request that has not been received");
        }
    };

    // use digest to go through HotStuff consensus
    hotstuff_interface.propose(digest, execb);
  }
}

    
bool Replica::sendMessageToPrimary(const ::google::protobuf::Message& msg) {
  int primaryIdx = config.GetLeaderIndex(currentView);
  if (signMessages) {
    proto::SignedMessage signedMsg;
    // SignMessage(msg, keyManager->GetPrivateKey(id), id, signedMsg);
    Panic("Unimplemented");
    return transport->SendMessageToReplica(this, groupIdx, primaryIdx, signedMsg);
  } else {
    return transport->SendMessageToReplica(this, groupIdx, primaryIdx, msg);
  }
}

bool Replica::sendMessageToAll(const ::google::protobuf::Message& msg) {

  if (signMessages) {
    // ::google::protobuf::Message* copy = msg.New();
    // copy->CopyFrom(msg);
    // transport->DispatchTP([this, copy] {
    //   proto::SignedMessage* signedMsg = new proto::SignedMessage();
    //   SignMessage(*copy, this->keyManager->GetPrivateKey(this->id), this->id, *signedMsg);
    //   delete copy;
    //   return (void*) signedMsg;
    // }, [this](void* ret) {
    //   proto::SignedMessage* signedMsg = (proto::SignedMessage*) ret;
    //   // send to everyone and to me
    //   this->transport->SendMessageToGroup(this, this->groupIdx, *signedMsg);
    //   this->transport->SendMessageToReplica(this, this->groupIdx, this->idx, *signedMsg);
    //   delete signedMsg;
    // });
    // return true;
    proto::SignedMessage signedMsg;
    CreateHMACedMessage(msg, signedMsg);

    return this->transport->SendMessageToGroup(this, groupIdx, signedMsg) &&
           this->transport->SendMessageToReplica(this, groupIdx, idx, signedMsg);
  } else {
    // send to everyone and to me
    return transport->SendMessageToGroup(this, groupIdx, msg) &&
           transport->SendMessageToReplica(this, groupIdx, idx, msg);
  }
}


void Replica::sendBatchedPreprepare() {
  proto::BatchedRequest batchedRequest;
  for (const auto& pair : pendingBatchedDigests) {
    (*batchedRequest.mutable_digests())[pair.first] = pair.second;
  }
  pendingBatchedDigests.clear();
  nextBatchNum = 0;
  // send the batched preprepare to everyone
  sendMessageToAll(batchedRequest);

  // send a preprepare for the batched request
  proto::Preprepare preprepare;
  string digest = BatchedDigest(batchedRequest);
  uint64_t seqnum = nextSeqNum++;
  preprepare.set_seqnum(seqnum);
  preprepare.set_viewnum(currentView);
  preprepare.set_digest(digest);

  SendPreprepare(seqnum, preprepare);
}

void Replica::SendPreprepare(uint64_t seqnum, const proto::Preprepare& preprepare) {
  // send preprepare to everyone
  sendMessageToAll(preprepare);

  // wait 300 ms for tx to complete
  seqnumCommitTimers[seqnum] = transport->Timer(2000, [this, seqnum, preprepare]() {
    Debug("Primary commit timer expired, resending preprepare");
    stats->Increment("prim_expired",1);
    this->SendPreprepare(seqnum, preprepare);
  });
}

void Replica::HandleBatchedRequest(const TransportAddress &remote,
                               proto::BatchedRequest &request) {
  Debug("Handling batched request message");

  string digest = BatchedDigest(request);
  batchedRequests[digest] = request;

  // this could be the message that allows us to execute a slot
  executeSlots();
}

void Replica::HandlePreprepare(const TransportAddress &remote,
                                  const proto::Preprepare &preprepare,
                                const proto::SignedMessage& signedMsg) {
  Debug("Handling preprepare message");


  int primaryIdx = config.GetLeaderIndex(currentView);
  int primaryId = groupIdx * config.n + primaryIdx;

  if (signMessages) {
    // make sure this message is from this shard
    if (signedMsg.replica_id() / config.n != (uint64_t) groupIdx) {
      stats->Increment("invalid_pp_group",1);
      return;
    }
    // make sure id is good
    if ((int) signedMsg.replica_id() != primaryId) {
      return;
    }
    // make sure the primary isn't equivocating
    if (!slots.setPreprepare(preprepare, signedMsg.replica_id(), signedMsg.signature())) {
      stats->Increment("invalid_pp_hash",1);
      return;
    }
  } else {
    if (!slots.setPreprepare(preprepare)) {
      return;
    }
  }

  uint64_t seqnum = preprepare.seqnum();
  Debug("PP (preprepare) seq num: %lu", seqnum);
  uint64_t viewnum = preprepare.viewnum();
  string digest = preprepare.digest();
  startActionTimer(seqnum, viewnum, digest);

  // if I am the primary, I shouldn't be sending a prepare
  if (idx != primaryIdx) {
    // Multicast prepare to everyone
    proto::Prepare prepare;
    prepare.set_seqnum(seqnum);
    prepare.set_viewnum(viewnum);
    prepare.set_digest(digest);

    if (primaryCoordinator) {
      sendMessageToPrimary(prepare);
    } else {
      sendMessageToAll(prepare);
    }
  }

  testSlot(seqnum, viewnum, digest, true);
}

void Replica::HandlePrepare(const TransportAddress &remote,
                               const proto::Prepare &prepare,
                             const proto::SignedMessage& signedMsg) {
  Debug("Handling prepare message");
  if (signMessages) {
    // make sure this message is from this shard
    if (signedMsg.replica_id() / config.n != (uint64_t) groupIdx) {
      stats->Increment("invalid_p_group",1);
      return;
    }
    if (!slots.addPrepare(prepare, signedMsg.replica_id(), signedMsg.signature())) {
      stats->Increment("invalid_pp_hash",1);
      return;
    }
  } else {
    if (!slots.addPrepare(prepare)) {
      return;
    }
  }

  uint64_t seqnum = prepare.seqnum();
  Debug("prepare seq num: %lu", seqnum);
  uint64_t viewnum = prepare.viewnum();
  string digest = prepare.digest();

  testSlot(seqnum, viewnum, digest, true);
}

void Replica::HandleCommit(const TransportAddress &remote,
                              const proto::Commit &commit,
                            const proto::SignedMessage& signedMsg) {
  Debug("Handling commit message");

  if (signMessages) {
    // make sure this message is from this shard
    if (signedMsg.replica_id() / config.n != (uint64_t) groupIdx) {
      stats->Increment("invalid_c_group",1);
      return;
    }
    if (!slots.addCommit(commit, signedMsg.replica_id(), signedMsg.signature())) {
      stats->Increment("invalid_c_hash",1);
      return;
    }
  } else {
    if (!slots.addCommit(commit)) {
      return;
    }
  }

  uint64_t seqnum = commit.seqnum();
  Debug("commit seq num: %lu", seqnum);
  uint64_t viewnum = commit.viewnum();
  string digest = commit.digest();

  testSlot(seqnum, viewnum, digest, false);
}

void Replica::HandleGrouped(const TransportAddress &remote,
                          const proto::GroupedSignedMessage &msg) {

  Debug("Handling grouped message");

  int primaryIdx = config.GetLeaderIndex(currentView);
  // primary should be the one that sent the grouped message
  if (idx != primaryIdx) {
    proto::PackedMessage packedMsg;
    if (packedMsg.ParseFromString(msg.packed_msg())) {
      proto::Prepare prepare;
      proto::Commit commit;

      proto::SignedMessage signedMsg;
      signedMsg.set_packed_msg(msg.packed_msg());

      if (packedMsg.type() == prepare.GetTypeName()) {
        if (prepare.ParseFromString(packedMsg.msg())) {
          for (const auto& pair : msg.signatures()) {
            Debug("ungrouped prepare for %lu", pair.first);
            signedMsg.set_replica_id(pair.first);
            signedMsg.set_signature(pair.second);

            if (!signMessages || CheckSignature(signedMsg, keyManager)) {
              HandlePrepare(remote, prepare, signedMsg);
            } else {
              Debug("Failed to validate prepare signature for %lu", pair.first);
            }
          }
        }
      } else if (packedMsg.type() == commit.GetTypeName()) {
        if (commit.ParseFromString(packedMsg.msg())) {
          for (const auto& pair : msg.signatures()) {
            Debug("ungrouped prepare for %lu", pair.first);
            signedMsg.set_replica_id(pair.first);
            signedMsg.set_signature(pair.second);

            if (!signMessages || CheckSignature(signedMsg, keyManager)) {
              HandleCommit(remote, commit, signedMsg);
            } else {
              Debug("Failed to validate prepare signature for %lu", pair.first);
            }
          }
        }
      }
    }
  }

}

// tests to see if slot can be executed. Called after receiving preprepare
// prepare, and commit because these messages can arrive in arbitrary order
// and commit messages may be received before preprepares on a sufficiently
// odd network
void Replica::testSlot(uint64_t seqnum, uint64_t viewnum, string digest, bool gotPrepare) {
  // wait for 2f prepare + preprepare all matching and then send commit to
  // everyone start timer for 2f+1 commits
  // the gotPrepare just makes it so that commits don't cause commits to be sent
  // which would just loop forever
  if (gotPrepare && slots.Prepared(seqnum, viewnum, config.f)) {
    Debug("Sending commit to everyone");
    cancelActionTimer(seqnum, viewnum, digest);
    startActionTimer(seqnum, viewnum, digest);

    // Multicast commit to everyone
    proto::Commit commit;
    commit.set_seqnum(seqnum);
    commit.set_viewnum(viewnum);
    commit.set_digest(digest);

    if (primaryCoordinator) {
      int primaryIdx = config.GetLeaderIndex(currentView);
      if (idx == primaryIdx) {
        Debug("Sending prepare proof");
        // send prepare proof to all replicas
        proto::GroupedSignedMessage proof = slots.getPrepareProof(seqnum, viewnum, digest);
        sendMessageToAll(proof);
      }

      sendMessageToPrimary(commit);
    } else {
      sendMessageToAll(commit);
    }
  }

  // wait for 2f+1 matching commit messages, then mark message as committed,
  // clear timer
  if (slots.CommittedLocal(seqnum, viewnum, config.f)) {
    Debug("Committed message");
    cancelActionTimer(seqnum, viewnum, digest);

    if (primaryCoordinator) {
      int primaryIdx = config.GetLeaderIndex(currentView);
      if (idx == primaryIdx) {
        Debug("Sending commit proof");
        // send commit proof to all replicas
        proto::GroupedSignedMessage proof = slots.getCommitProof(seqnum, viewnum, digest);
        sendMessageToAll(proof);
      }
    }

    pendingExecutions[seqnum] = digest;

    Debug("seqnum: %lu", seqnum);

    executeSlots();
  }
}

void Replica::executeSlots() {
  Debug("exec seq num: %lu", execSeqNum);
  while(pendingExecutions.find(execSeqNum) != pendingExecutions.end()) {
    // cancel the commit timer
    transport->CancelTimer(seqnumCommitTimers[execSeqNum]);
    seqnumCommitTimers.erase(execSeqNum);

    string batchDigest = pendingExecutions[execSeqNum];
    // only execute when we have the batched request
    if (batchedRequests.find(batchDigest) != batchedRequests.end()) {
      string digest = (*batchedRequests[batchDigest].mutable_digests())[execBatchNum];
      DebugHash(digest);
      // only execute if we have the full request
      if (requests.find(digest) != requests.end()) {
        stats->Increment("exec_request",1);
        Debug("executing seq num: %lu %lu", execSeqNum, execBatchNum);
        proto::PackedMessage packedMsg = requests[digest];
        std::vector<::google::protobuf::Message*> replies = app->Execute(packedMsg.type(), packedMsg.msg());
        for (const auto& reply : replies) {
          if (reply != nullptr) {
            Debug("Sending reply");
            stats->Increment("execs_sent",1);
            transport->SendMessage(this, *replyAddrs[digest], *reply);
            delete reply;
          } else {
            Debug("Invalid execution");
          }
        }

        execBatchNum++;
        if ((int) execBatchNum >= batchedRequests[batchDigest].digests_size()) {
          Debug("Done executing batch");
          execBatchNum = 0;
          execSeqNum++;
        }
      } else {
        Debug("request from batch %lu not yet received", execSeqNum);
        if (requestTx) {
          stats->Increment("req_txn",1);
          proto::RequestRequest rr;
          rr.set_digest(digest);
          int primaryIdx = config.GetLeaderIndex(currentView);
          if (primaryIdx == idx) {
            stats->Increment("primary_req_txn",1);
          }
          transport->SendMessageToReplica(this, groupIdx, primaryIdx, rr);
        }
        break;
      }
    } else {
      Debug("Batch request not yet received");
      if (requestTx) {
        stats->Increment("req_batch",1);
        proto::RequestRequest rr;
        rr.set_digest(batchDigest);
        int primaryIdx = config.GetLeaderIndex(currentView);
        transport->SendMessageToReplica(this, groupIdx, primaryIdx, rr);
      }
      break;
    }
  }
}

void Replica::startActionTimer(uint64_t seq_num, uint64_t viewnum, std::string digest) {
  // actionTimers[seq_num][viewnum][digest] = transport->Timer(10, [seq_num, viewnum, digest, this]() {
  //   Debug("action timer expired, sending");
  //   proto::ABRequest abreq;
  //   abreq.set_seqnum(seq_num);
  //   abreq.set_viewnum(viewnum);
  //   abreq.set_digest(digest);
  //   int primaryIdx = this->config.GetLeaderIndex(this->currentView);
  //   this->stats->Increment("sent_ab_req",1);
  //   this->transport->SendMessageToReplica(this, this->groupIdx, primaryIdx, abreq);
  // });
}

void Replica::cancelActionTimer(uint64_t seq_num, uint64_t viewnum, std::string digest) {
  // if (actionTimers[seq_num][viewnum].find(digest) != actionTimers[seq_num][viewnum].end()) {
  //   transport->CancelTimer(actionTimers[seq_num][viewnum][digest]);
  //   actionTimers[seq_num][viewnum].erase(digest);
  // }
}


}  // namespace hotstuffstore
