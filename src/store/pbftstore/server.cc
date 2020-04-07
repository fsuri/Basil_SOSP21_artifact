#include "store/pbftstore/server.h"
#include "store/pbftstore/common.h"
#include <iostream>

namespace pbftstore {

using namespace std;

Server::Server(KeyManager *keyManager, int groupIdx, int myId, bool signMessages, bool validateReads) :
keyManager(keyManager), groupIdx(groupIdx), myId(myId), signMessages(signMessages), validateReads(validateReads){

}

Server::~Server() {}

::google::protobuf::Message* Server::Execute(const std::string& type, const std::string& msg, proto::CommitProof &&commitProof) {
  cout << "Execute: " << type << endl;

  proto::Transaction transaction;
  if (type == transaction.GetTypeName()) {
    transaction.ParseFromString(msg);

    proto::TransactionDecision* decision = new proto::TransactionDecision();
    std::string digest = TransactionDigest(transaction);
    decision->set_txn_digest(digest);
    decision->set_shard_id(groupIdx);
    // TODO actually do OCC check and add to prepared list
    // OCC check
    if (true) {
      decision->set_status(0);
    } else {
      decision->set_status(1);
    }
    // Send decision to client
    return decision;
  }
  return nullptr;
}

::google::protobuf::Message* Server::HandleMessage(const std::string& type, const std::string& msg) {
  cout << "Handle" << type << endl;

  proto::Read read;
  proto::GroupedDecision gdecision;

  if (type == read.GetTypeName()) {
    proto::ReadReply* readReply = new proto::ReadReply();
    readReply->set_req_id(read.req_id());
    readReply->set_status(1);
    readReply->set_key(read.key());
    // TODO actually do read

    return readReply;
  } else if (type  == gdecision.GetTypeName()) {
    // TODO check the decision, apply the tx if it is valid, return response
    proto::GroupedDecisionAck* groupedDecisionAck = new proto::GroupedDecisionAck();

    groupedDecisionAck->set_status(0);

    return groupedDecisionAck;
  }

  return nullptr;
}

void Server::Load(const std::string &key, const std::string &value,
    const Timestamp timestamp) {
      Panic("Unimplemented");
}

Stats &Server::GetStats() {
  return stats;
}

}
