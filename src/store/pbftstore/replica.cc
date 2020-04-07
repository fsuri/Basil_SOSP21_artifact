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
  // printf("Received a message\n");
  proto::SignedMessage signedMessage;
  string type;
  string data;
  uint64_t replica_id = -1;

  if (t == signedMessage.GetTypeName()) {
    printf("Received signed message\n");
    if (!signedMessage.ParseFromString(d)) {
      return;
    }

    if (!ValidateSignedMessage(signedMessage, keyManager, data, type)) {
      return;
    }
    replica_id = signedMessage.replica_id();
    printf("Message is valid!\n");
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
    preprepare.ParseFromString(data);
    HandlePreprepare(remote, preprepare, replica_id);
  } else if (type == prepare.GetTypeName()) {
    prepare.ParseFromString(data);
    HandlePrepare(remote, prepare, replica_id);
  } else if (type == commit.GetTypeName()) {
    commit.ParseFromString(data);
    uint64_t seqnum = commit.seqnum();
    uint64_t viewnum = commit.viewnum();
    string digest = commit.digest();

    if (replica_id == (uint64_t) -1) {
      replica_id = slots.getNumCommitted(seqnum, viewnum, digest);
    }

    if (t == signedMessage.GetTypeName()) {
      signedCommitGroups[seqnum][viewnum][digest][replica_id] = signedMessage;
    } else {
      commitGroups[seqnum][viewnum][digest][replica_id] = commit;
    }

    HandleCommit(remote, commit, replica_id);
  } else {
    cout << "Sending request to app" << endl;
    ::google::protobuf::Message* reply = app->HandleMessage(type, data);
    if (reply != nullptr) {
      transport->SendMessage(this, remote, *msg);
      delete msg;
    } else {
      cout << "Invalid request of type " << type << endl;
    }

  }
}

void Replica::HandleRequest(const TransportAddress &remote,
                               const proto::Request &request) {
  printf("Handling request message\n");

  if (!slots.requestExists(request)) {
    cout << "new request: " << request.packed_msg().type() << endl;

    slots.addVerifiedRequest(request);

    // clone remote mapped to request for reply
    std::string digest = RequestDigest(request);
    replyAddrs[digest] = remote.clone();

    int currentPrimary = config.GetLeaderIndex(view);
    if (currentPrimary == myId) {
      // If I am the primary, send preprepare to everyone
      proto::Preprepare preprepare;
      preprepare.set_seqnum(seqnum++);
      preprepare.set_viewnum(view);
      preprepare.set_digest(digest);

      if (signMessages) {
        proto::SignedMessage signedMessage;
        SignMessage(preprepare, keyManager->GetPrivateKey(myId), myId, signedMessage);
        // send to everyone and to me
        transport->SendMessageToGroup(this, groupIdx, signedMessage);
        transport->SendMessageToReplica(this, groupIdx, myId, signedMessage);
      } else {
        // send to everyone and to me
        transport->SendMessageToGroup(this, groupIdx, preprepare);
        transport->SendMessageToReplica(this, groupIdx, myId, preprepare);
      }
    }
  }
}

void Replica::HandlePreprepare(const TransportAddress &remote,
                                  const proto::Preprepare &preprepare, uint64_t replica_id) {
  printf("Handling preprepare message\n");

  uint64_t primaryId = config.GetLeaderIndex(view);

  uint64_t seqnum = preprepare.seqnum();
  uint64_t viewnum = preprepare.viewnum();
  string digest = preprepare.digest();

  if (replica_id == (uint64_t) -1) {
    replica_id = slots.getNumPrepared(seqnum, viewnum, digest);
  } else {
    if (replica_id != primaryId) {
      // only accept preprepares from the current primary
      return;
    }
  }
  if(!slots.setVerifiedPreprepare(primaryId, preprepare)) {
    // The primary is equivocating, don't accept the preprepare
    return;
  }

  // Multicast prepare to everyone
  proto::Prepare prepare;
  prepare.set_seqnum(seqnum);
  prepare.set_viewnum(viewnum);
  prepare.set_digest(digest);

  if (signMessages) {
    proto::SignedMessage signedMessage;
    SignMessage(prepare, keyManager->GetPrivateKey(myId), myId, signedMessage);
    // send to everyone and to me
    transport->SendMessageToGroup(this, groupIdx, signedMessage);
    transport->SendMessageToReplica(this, groupIdx, myId, signedMessage);
  } else {
    // send to everyone and to me
    transport->SendMessageToGroup(this, groupIdx, prepare);
    transport->SendMessageToReplica(this, groupIdx, myId, prepare);
  }
}

void Replica::HandlePrepare(const TransportAddress &remote,
                               const proto::Prepare &prepare, uint64_t replica_id) {
  printf("Handling prepare message\n");

  uint64_t seqnum = prepare.seqnum();
  uint64_t viewnum = prepare.viewnum();
  string digest = prepare.digest();

  if (replica_id == (uint64_t) -1) {
    replica_id = slots.getNumPrepared(seqnum, viewnum, digest);
  }
  slots.setVerifiedPrepare(prepare, replica_id);

  // wait for 2f prepare + preprepare all matching and then send commit to
  // everyone start timer for 2f+1 commits
  if (slots.Prepared(seqnum, viewnum, config.f) && sentCommits[seqnum].find(viewnum) == sentCommits[seqnum].end()) {
    cout << "Sending commit to everyone" << endl;

    sentCommits[seqnum].insert(viewnum);

    // Multicast commit to everyone
    proto::Commit commit;
    commit.set_seqnum(seqnum);
    commit.set_viewnum(viewnum);
    commit.set_digest(digest);

    if (signMessages) {
      proto::SignedMessage signedMessage;
      SignMessage(commit, keyManager->GetPrivateKey(myId), myId, signedMessage);
      // send to everyone and to me
      transport->SendMessageToGroup(this, groupIdx, signedMessage);
      transport->SendMessageToReplica(this, groupIdx, myId, signedMessage);
    } else {
      // send to everyone and to me
      transport->SendMessageToGroup(this, groupIdx, commit);
      transport->SendMessageToReplica(this, groupIdx, myId, commit);
    }
  }
}

void Replica::HandleCommit(const TransportAddress &remote,
                              const proto::Commit &commit, uint64_t replica_id) {
  printf("Handling commit message\n");

  uint64_t seqnum = commit.seqnum();
  uint64_t viewnum = commit.viewnum();
  string digest = commit.digest();

  slots.setVerifiedCommit(commit, replica_id);

  // wait for 2f+1 matching commit messages, then mark message as committed,
  // clear timer
  cout << config.f << endl;
  if (slots.CommittedLocal(seqnum, viewnum, config.f)) {
    cout << "Committed message with type: " << slots.getRequestMessage(digest)->type() << endl;
    std::pair<uint64_t, std::string> view_and_digest(viewnum, digest);
    pendingSeqNum[seqnum] = view_and_digest;

    cout << execSeqNum << " " << seqnum << endl;

    while(pendingSeqNum.find(execSeqNum) != pendingSeqNum.end()) {
      std::string digest = pendingSeqNum[execSeqNum].second;
      proto::PackedMessage* msg = slots.getRequestMessage(digest);
      proto::CommitProof commitProof;
      if (signMessages) {
        auto& map = *commitProof.mutable_signed_commits()->mutable_commits();
        for (auto const& x : signedCommitGroups[seqnum][viewnum][digest]) {
          map[x.first] = x.second;
        }
      } else {
        auto& map = *commitProof.mutable_commits()->mutable_commits();
        for (auto const& x : commitGroups[seqnum][viewnum][digest]) {
          map[x.first] = x.second;
        }
      }
      ::google::protobuf::Message* reply = app->Execute(msg->type(), msg->msg(), std::move(commitProof));
      if (reply != nullptr) {
        cout << "Sending reply" << endl;
        transport->SendMessage(this, *replyAddrs[digest], *reply);
        delete reply;
      } else {
        cout << "Invalid execution" << endl;
      }

      execSeqNum++;
    }
  }
}
}  // namespace pbftstore
