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
}

Replica::~Replica() {}

void Replica::ReceiveMessage(const TransportAddress &remote, const string &t,
                          const string &d, void *meta_data) {
  printf("Received a message\n");
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
    HandleCommit(remote, commit, replica_id);
  } else {
    Panic("Received unexpected message type in IR proto: %s", type.c_str());
  }
}

void Replica::HandleRequest(const TransportAddress &remote,
                               const proto::Request &request) {
  printf("Handling request message\n");

  if (!slots.requestExists(request)) {

    slots.addVerifiedRequest(request);

    int currentPrimary = config.GetLeaderIndex(view);
    if (currentPrimary == myId) {
      // forward the message to everyone
      transport->SendMessageToGroup(this, groupIdx, request);

      // If I am the primary, send preprepare to everyone
      proto::Preprepare preprepare;
      preprepare.set_seqnum(seqnum++);
      preprepare.set_viewnum(view);
      preprepare.set_digest(RequestDigest(request));

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
    } else {
      // Otherwise, forward to current primary
      transport->SendMessageToReplica(this, groupIdx, currentPrimary, request);
      // TODO start a timer for preprepare
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
  slots.setVerifiedPreprepare(primaryId, preprepare);

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
  if (slots.Prepared(seqnum, viewnum, config.f)) {
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

  if (replica_id == (uint64_t) -1) {
    replica_id = slots.getNumCommitted(seqnum, viewnum, digest);
  }
  slots.setVerifiedCommit(commit, replica_id);

  // wait for 2f+1 matching commit messages, then mark message as committed,
  // clear timer
  if (slots.CommittedLocal(seqnum, viewnum, config.f)) {
    cout << "Committed message with type: " << slots.getRequestMessage(digest)->type() << endl;
  }
}
}  // namespace pbftstore
