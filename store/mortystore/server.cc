#include "store/mortystore/server.h"

#include <sstream>

#include "store/mortystore/common.h"

#include <google/protobuf/util/message_differencer.h>

namespace mortystore {

Server::Server(const transport::Configuration &config, int groupIdx, int idx,
    Transport *transport) : config(config), idx(idx), transport(transport) {
  transport->Register(this, config, groupIdx, idx);
}

Server::~Server() {
}

void Server::ReceiveMessage(const TransportAddress &remote,
      const std::string &type, const std::string &data, void *meta_data) {
  proto::Read read;
  proto::Write write;
  proto::Prepare prepare;
  proto::KO ko;
  proto::Commit commit;
  proto::Abort abort;

  if (type == read.GetTypeName()) {
    read.ParseFromString(data);
    HandleRead(remote, read);
  } else if (type == write.GetTypeName()) {
    write.ParseFromString(data);
    HandleWrite(remote, write);
  } else if (type == prepare.GetTypeName()) {
    prepare.ParseFromString(data);
    HandlePrepare(remote, prepare);
  } else if (type == ko.GetTypeName()) {
    ko.ParseFromString(data);
    HandleKO(remote, ko);
  } else if (type == commit.GetTypeName()) {
    commit.ParseFromString(data);
    HandleCommit(remote, commit);
  } else if (type == abort.GetTypeName()) {
    abort.ParseFromString(data);
    HandleAbort(remote, abort);
  } else {
    Panic("Received unexpected message type: %s", type.c_str());
  }
}

void Server::Load(const std::string &key, const std::string &value,
    const Timestamp timestamp) {
}

void Server::HandleRead(const TransportAddress &remote, const proto::Read &msg) {
  if (committed_txn_ids.find(msg.branch().txn().id()) != committed_txn_ids.end()) {
    // msg is for already committed txn
    return;
  }

  txn_coordinators[msg.branch().txn().id()] = &remote;

  generator.AddPendingRead(msg.key(), msg.branch());

  SendBranchReplies(msg.branch(), proto::OperationType::READ, msg.key());
}

void Server::HandleWrite(const TransportAddress &remote, const proto::Write &msg) {
  if (committed_txn_ids.find(msg.branch().txn().id()) != committed_txn_ids.end()) {
    // msg is for already committed txn
    return;
  }

  if (Message_DebugEnabled(__FILE__)) {
    std::stringstream ss;
    ss << "Received write: ";
    PrintBranch(msg.branch(), ss);
    Debug("%s", ss.str().c_str());
  }

  txn_coordinators[msg.branch().txn().id()] = &remote;

  generator.AddPendingWrite(msg.key(), msg.branch());

  SendBranchReplies(msg.branch(), proto::OperationType::WRITE, msg.key());
}

void Server::HandlePrepare(const TransportAddress &remote, const proto::Prepare &msg) {
  if (committed_txn_ids.find(msg.branch().txn().id()) != committed_txn_ids.end()) {
    // msg is for already committed txn
    return;
  }

  if (!CheckBranch(remote, msg.branch())) {
    waiting.push_back(msg.branch());
  }
}

void Server::HandleKO(const TransportAddress &remote, const proto::KO &msg) {
  if (committed_txn_ids.find(msg.branch().txn().id()) != committed_txn_ids.end()) {
    // msg is for already committed txn
    return;
  }

  auto itr = std::find_if(prepared.begin(), prepared.end(), [&](const proto::Transaction &other) {
          return google::protobuf::util::MessageDifferencer::Equals(msg.branch().txn(), other);
        });
  auto jtr = prepared_txn_ids.find(msg.branch().txn().id());
  if (jtr != prepared_txn_ids.end()) {
    prepared_txn_ids.erase(jtr);
  }
  if (itr != prepared.end()) {
    prepared.erase(itr);
    ++itr;
    for (; itr != prepared.end(); ++itr) {
      // TODO: this is probably unsafe, unpreparing all transactions that prepared
      // after this koed txn
      prepared.erase(itr);
    }
  }
}

void Server::HandleCommit(const TransportAddress &remote, const proto::Commit &msg) {
  if (committed_txn_ids.find(msg.branch().txn().id()) != committed_txn_ids.end()) {
    // msg is for already committed txn
    return;
  }

  committed.push_back(msg.branch().txn());
  committed_txn_ids.insert(msg.branch().txn().id());
  
  generator.ClearPending(msg.branch().txn().id());

  for (auto itr = waiting.begin(); itr != waiting.end(); ) {
    if (CheckBranch(*txn_coordinators[itr->txn().id()],
          *itr)) {
      waiting.erase(itr);
      for (auto shard : itr->shards()) {
        transport->SendMessage(this, *shards[shard], msg);
      }
    } else {
      ++itr;
    }
  }
}

void Server::HandleAbort(const TransportAddress &remote, const proto::Abort &msg) {
  generator.ClearPending(msg.branch().id());
}

bool Server::CheckBranch(const TransportAddress &addr, const proto::Branch &branch) {
  if (prepared_txn_ids.find(branch.txn().id()) != prepared_txn_ids.end()) {
    // cannot prepare more than one branch for the same txn
    Debug("Transaction %lu already prepared on different branch.", branch.txn().id());
    proto::PrepareKO reply;
    *reply.mutable_branch() = branch;
    transport->SendMessage(this, addr, reply);
    return true;
  } else if (CommitCompatible(branch, prepared, prepared_txn_ids)) {
    prepared.push_back(branch.txn());
    prepared_txn_ids.insert(branch.txn().id());
    proto::PrepareOK reply;
    *reply.mutable_branch() = branch;
    transport->SendMessage(this, addr, reply);
    return true;
  } else if (!WaitCompatible(branch, prepared)) {
    if (Message_DebugEnabled(__FILE__)) {
      std::stringstream ss;
      ss << "Branch not compatible with prepared." << std::endl;
      ss << "Branch: " << std::endl;
      PrintBranch(branch, ss);
      ss << std::endl << "Prepared: " << std::endl;
      PrintTransactionList(prepared, ss);
      Debug("%s", ss.str().c_str());
    }

    proto::PrepareKO reply;
    *reply.mutable_branch() = branch;
    transport->SendMessage(this, addr, reply);
    return true;
  } else {
    return false;
  }
}

void Server::SendBranchReplies(const proto::Branch &init,
    proto::OperationType type, const std::string &key) {
  std::vector<proto::Branch> generated_branches;
  generator.GenerateBranches(init, type, key, committed, generated_branches);
  for (const proto::Branch &branch : generated_branches) {
    const proto::Operation &op = branch.txn().ops()[branch.txn().ops().size() - 1];
    if (op.type() == proto::OperationType::READ) {
      std::string val;
      ValueOnBranch(branch, op.key(), val);
      proto::ReadReply reply;
      *reply.mutable_branch() =  branch;
      reply.set_key(op.key());
      reply.set_value(val);
      transport->SendMessage(this, *txn_coordinators[branch.txn().id()], reply);
    } else {
      proto::WriteReply reply;
      *reply.mutable_branch() = branch;
      reply.set_key(op.key());
      reply.set_value(op.val());
      transport->SendMessage(this, *txn_coordinators[branch.txn().id()], reply);
    }
  }
}

} // namespace mortystore
