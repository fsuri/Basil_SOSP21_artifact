#include "store/pbftstore/replica.h"

namespace pbftstore {

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
  App *app, int groupIdx, int myId, bool signMessages, Transport *transport)
    : config(config), keyManager(keyManager), app(app),
    groupIdx(groupIdx), myId(myId), signMessages(signMessages),
    transport(transport) {
  transport->Register(this, config, groupIdx, myId);

  // intial view
  view = 0;
  // initial seqnum
  seqnum = 0;
  execSeqNum = 0;
}

Replica::~Replica() {}

void Replica::ReceiveMessage(const TransportAddress &remote, const string &t,
                          const string &d, void *meta_data) {
  // Debug("Received a message");
  proto::SignedMessage signedMessage;
  string type;
  string data;
  bool recvSignedMessage = false;

  if (t == signedMessage.GetTypeName()) {
    Debug("Received signed message");
    if (!signedMessage.ParseFromString(d)) {
      return;
    }

    if (!ValidateSignedMessage(signedMessage, keyManager, data, type)) {
      return;
    }
    recvSignedMessage = true;
    Debug("Message is valid!");
  } else {
    type = t;
    data = d;
  }

  proto::Request request;
  proto::Preprepare preprepare;
  proto::Prepare prepare;
  proto::Commit commit;

  if (type == request.GetTypeName()) {
    request.ParseFromString(data);
    HandleRequest(remote, request);
  } else if (type == preprepare.GetTypeName()) {
    if (signMessages && !recvSignedMessage) {
      return;
    }

    preprepare.ParseFromString(data);
    HandlePreprepare(remote, preprepare, signedMessage);
  } else if (type == prepare.GetTypeName()) {
    prepare.ParseFromString(data);
    if (signMessages && !recvSignedMessage) {
      return;
    }

    HandlePrepare(remote, prepare, signedMessage);
  } else if (type == commit.GetTypeName()) {
    commit.ParseFromString(data);
    if (signMessages && !recvSignedMessage) {
      return;
    }
    
    HandleCommit(remote, commit, signedMessage);
  } else {
    Debug("Sending request to app");
    ::google::protobuf::Message* reply = app->HandleMessage(type, data);
    if (reply != nullptr) {
      transport->SendMessage(this, remote, *reply);
      delete reply;
    } else {
      Debug("Invalid request of type %s", type.c_str());
    }

  }
}

void Replica::sendMessageToAll(const ::google::protobuf::Message& msg) {
  if (signMessages) {
    proto::SignedMessage signedMessage;
    SignMessage(msg, keyManager->GetPrivateKey(myId), myId, signedMessage);
    // send to everyone and to me
    transport->SendMessageToGroup(this, groupIdx, signedMessage);
    transport->SendMessageToReplica(this, groupIdx, myId, signedMessage);
  } else {
    // send to everyone and to me
    transport->SendMessageToGroup(this, groupIdx, msg);
    transport->SendMessageToReplica(this, groupIdx, myId, msg);
  }
}

void Replica::HandleRequest(const TransportAddress &remote,
                               const proto::Request &request) {
  Debug("Handling request message");

  std::string digest = request.digest();

  if (requests.find(digest) == requests.end()) {
    Debug("new request: %s", request.packed_msg().type().c_str());

    requests[digest] = request.packed_msg();

    // clone remote mapped to request for reply
    replyAddrs[digest] = remote.clone();

    int currentPrimary = config.GetLeaderIndex(view);
    if (currentPrimary == myId) {
      // If I am the primary, send preprepare to everyone
      proto::Preprepare preprepare;
      preprepare.set_seqnum(seqnum++);
      preprepare.set_viewnum(view);
      preprepare.set_digest(digest);

      sendMessageToAll(preprepare);
    }
  }
}

void Replica::HandleBatchedRequest(const TransportAddress &remote,
                               const proto::BatchedRequest &request) {
  Debug("Handling batched request message");
}

void Replica::HandlePreprepare(const TransportAddress &remote,
                                  const proto::Preprepare &preprepare,
                                const proto::SignedMessage& signedMsg) {
  Debug("Handling preprepare message");

  uint64_t primaryId = config.GetLeaderIndex(view);

  if (signMessages) {
    // make sure id is good
    if (signedMsg.replica_id() != primaryId ||
        !slots.setPreprepare(preprepare, signedMsg.replica_id(), signedMsg.signature())) {
      return;
    }
  } else {
    if (!slots.setPreprepare(preprepare)) {
      return;
    }
  }


  uint64_t seqnum = preprepare.seqnum();
  uint64_t viewnum = preprepare.viewnum();
  string digest = preprepare.digest();

  // Multicast prepare to everyone
  proto::Prepare prepare;
  prepare.set_seqnum(seqnum);
  prepare.set_viewnum(viewnum);
  prepare.set_digest(digest);

  sendMessageToAll(prepare);
}

void Replica::HandlePrepare(const TransportAddress &remote,
                               const proto::Prepare &prepare,
                             const proto::SignedMessage& signedMsg) {
  Debug("Handling prepare message");
  if (signMessages) {
    if (!slots.addPrepare(prepare, signedMsg.replica_id(), signedMsg.signature())) {
      return;
    }
  } else {
    if (!slots.addPrepare(prepare)) {
      return;
    }
  }

  uint64_t seqnum = prepare.seqnum();
  uint64_t viewnum = prepare.viewnum();
  string digest = prepare.digest();

  // wait for 2f prepare + preprepare all matching and then send commit to
  // everyone start timer for 2f+1 commits
  if (slots.Prepared(seqnum, viewnum, config.f) && sentCommits[seqnum].find(viewnum) == sentCommits[seqnum].end()) {
    Debug("Sending commit to everyone");

    sentCommits[seqnum].insert(viewnum);

    // Multicast commit to everyone
    proto::Commit commit;
    commit.set_seqnum(seqnum);
    commit.set_viewnum(viewnum);
    commit.set_digest(digest);

    sendMessageToAll(commit);
  }
}

void Replica::HandleCommit(const TransportAddress &remote,
                              const proto::Commit &commit,
                            const proto::SignedMessage& signedMsg) {
  Debug("Handling commit message");

  if (signMessages) {
    if (!slots.addCommit(commit, signedMsg.replica_id(), signedMsg.signature())) {
      return;
    }
  } else {
    if (!slots.addCommit(commit)) {
      return;
    }
  }


  uint64_t seqnum = commit.seqnum();
  uint64_t viewnum = commit.viewnum();
  string digest = commit.digest();

  // wait for 2f+1 matching commit messages, then mark message as committed,
  // clear timer
  if (slots.CommittedLocal(seqnum, viewnum, config.f)) {
    Debug("Committed message with type: %s", requests[digest].type().c_str());

    pendingExecutions[seqnum] = digest;

    Debug("exec seq num: %lu   seq num: %lu", execSeqNum, seqnum);

    while(pendingExecutions.find(execSeqNum) != pendingExecutions.end()) {
      Debug("executing seq num: %lu", execSeqNum);
      std::string digest = pendingExecutions[execSeqNum];
      proto::PackedMessage packedMsg = requests[digest];
      ::google::protobuf::Message* reply = app->Execute(packedMsg.type(), packedMsg.msg());
      if (reply != nullptr) {
        Debug("Sending reply");
        transport->SendMessage(this, *replyAddrs[digest], *reply);
        delete reply;
      } else {
        Debug("Invalid execution");
      }

      execSeqNum++;
    }
  }
}
}  // namespace pbftstore
