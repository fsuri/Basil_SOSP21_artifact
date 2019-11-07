#include "store/mortystore/client.h"

#include <sstream>

#include "store/mortystore/common.h"

namespace mortystore {

Client::Client(const std::string configPath, uint64_t client_id, int nShards, int nGroups,
    int closestReplica, Transport *transport, partitioner part) : client_id(client_id), nshards(nShards),
    ngroups(nGroups), transport(transport), part(part), lastReqId(0UL),
    prepareBranchIds(0UL), config(nullptr) {
  t_id = client_id << 20; 

  Debug("Initializing Morty client with id [%lu] %lu", client_id, nshards);

  std::ifstream configStream(configPath);
  if (configStream.fail()) {
    Panic("Unable to read configuration file: %s\n", configPath.c_str());
  }
  config = new transport::Configuration(configStream);

  /* Start a client for each shard. */
  for (uint64_t i = 0; i < nshards; i++) {
    sclients.push_back(new ShardClient(config, transport,
        client_id, i, closestReplica, this));
  }

  Debug("Morty client [%lu] created! %lu", client_id, nshards);

}

Client::~Client() {
  for (auto sclient : sclients) {
    delete sclient;
  }
  delete config;
}

void Client::Execute(AsyncTransaction *txn, execute_callback ecb) {
  PendingRequest *req = new PendingRequest(t_id);
  req->txn = txn;
  req->protoTxn.set_id(t_id);
  req->ecb = ecb;
  pendingReqs[t_id] = req;

  t_id++;
  proto::Branch branch;
  branch.set_id(0UL);
  *branch.mutable_txn() = req->protoTxn;
  ExecuteNextOperation(req, branch);
}


void Client::ExecuteNextOperation(PendingRequest *req, proto::Branch &branch) {
  ClientBranch clientBranch = GetClientBranch(branch);
  Operation op = req->txn->GetNextOperation(clientBranch.opCount,
      clientBranch.readValues);

  if (Message_DebugEnabled(__FILE__)) {
    std::stringstream ss;
    ss << "Executing next: ";
    PrintBranch(branch, ss);
    Debug("%s", ss.str().c_str());
  }

  switch (op.type) {
    case GET: {
      Get(branch, op.key);
      break;
    }
    case PUT: {
      Put(branch, op.key, op.value);
      break;
    }
    case COMMIT: {
      Commit(req, branch);
      break;
    }
    case ABORT: {
      Abort(branch);
      req->ecb(FAILED, std::map<std::string, std::string>());
      break;
    }
    default:
      NOT_REACHABLE();
  }
}

ClientBranch Client::GetClientBranch(const proto::Branch &branch) {
  ClientBranch clientBranch;
  clientBranch.opCount = branch.txn().ops_size();
  for (auto op : branch.txn().ops()) {
    if (op.type() == proto::OperationType::READ) {
      std::string val;
      ValueOnBranch(&branch, op.key(), val);
      clientBranch.readValues.insert(std::make_pair(op.key(), val));
    }
  }
  return clientBranch;
}

void Client::ValueOnBranch(const proto::Branch *branch, const std::string &key,
    std::string &val) {
  if (ValueInTransaction(branch->txn(), key, val)) {
    return;
  } else if (branch->seq().size() > 0) {
    for (auto itr = branch->seq().rbegin(); itr != branch->seq().rend(); ++itr) {
      if (ValueInTransaction(*itr, key, val)) {
        return;
      }
    }
  }
}

bool Client::ValueInTransaction(const proto::Transaction &txn, const std::string &key,
    std::string &val) {
  for (auto itr = txn.ops().rbegin(); itr != txn.ops().rend(); ++itr) {
    if (itr->type() == proto::OperationType::WRITE && itr->key() == key) {
      val = itr->val();
      return true;
    }
  }
  return false;
}


void Client::HandleReadReply(const TransportAddress &remote,
    const proto::ReadReply &msg, uint64_t shard) {
  auto itr = pendingReqs.find(msg.branch().txn().id());
  if (itr == pendingReqs.end()) {
    return;
  }

  proto::Branch branch(msg.branch());
  ExecuteNextOperation(itr->second, branch);
}

void Client::HandleWriteReply(const TransportAddress &remote,
    const proto::WriteReply &msg, uint64_t shard) {
  auto itr = pendingReqs.find(msg.branch().txn().id());
  if (itr == pendingReqs.end()) {
    return;
  }

  proto::Branch branch(msg.branch());
  ExecuteNextOperation(itr->second, branch);
}

void Client::HandlePrepareOK(const TransportAddress &remote,
    const proto::PrepareOK &msg, uint64_t shard) {
  auto itr = pendingReqs.find(msg.branch().txn().id());
  if (itr == pendingReqs.end()) {
    return;
  }

  itr->second->prepareOKs[msg.branch()]++;
  if (itr->second->prepareOKs[msg.branch()] == msg.branch().shards().size()) {
    proto::Commit commit;
    *commit.mutable_branch() = msg.branch();
    for (auto shard : msg.branch().shards()) {
      sclients[shard]->Commit(commit);
    }
    ClientBranch clientBranch = GetClientBranch(msg.branch());
    PendingRequest *req = itr->second;
    pendingReqs.erase(msg.branch().txn().id());
    req->ecb(SUCCESS, clientBranch.readValues);
    delete req;
  } else if (itr->second->prepareOKs[msg.branch()] +
      itr->second->prepareKOes[msg.branch()].size() ==
      msg.branch().shards().size()) {
    ProcessPrepareKOs(itr->second, msg.branch());
  }

}

void Client::HandleCommitReply(const TransportAddress &remote,
    const proto::CommitReply &msg, uint64_t shard) {
}

void Client::HandlePrepareKO(const TransportAddress &remote,
    const proto::PrepareKO &msg, uint64_t shard) {
  auto itr = pendingReqs.find(msg.branch().txn().id());
  if (itr == pendingReqs.end()) {
    return;
  }

  itr->second->prepareKOes[msg.branch()].push_back(msg);
  if (itr->second->prepareOKs[msg.branch()] +
      itr->second->prepareKOes[msg.branch()].size() ==
      msg.branch().shards().size()) {
    ProcessPrepareKOs(itr->second, msg.branch());
  }
}


void Client::Get(proto::Branch &branch, const std::string &key) {
  // Contact the appropriate shard to get the value.
  int i = part(key, nshards) % ngroups;
  if (std::find(branch.shards().begin(), branch.shards().end(),
        i) == branch.shards().end()) {
    branch.mutable_shards()->Add(i);
  }
  proto::Operation *op = branch.mutable_txn()->add_ops();
  op->set_type(proto::OperationType::READ);
  op->set_key(key);
  op->set_val("");

  proto::Read msg;
  *msg.mutable_branch() = branch;
  msg.set_key(key);

  if (Message_DebugEnabled(__FILE__)) {
    std::stringstream ss;
    ss << "Sending: ";
    PrintBranch(branch, ss);
    Debug("%s", ss.str().c_str());
  }

  sclients[i]->Read(msg);
}

void Client::Put(proto::Branch &branch, const std::string &key,
    const std::string &value) {
  // Contact the appropriate shard to get the value.
  int i = part(key, nshards) % ngroups;
  if (std::find(branch.shards().begin(), branch.shards().end(),
        i) == branch.shards().end()) {
    branch.mutable_shards()->Add(i);
  }
  proto::Operation *op = branch.mutable_txn()->add_ops();
  op->set_type(proto::OperationType::WRITE);
  op->set_key(key);
  op->set_val(value);

  proto::Write msg;
  *msg.mutable_branch() = branch;
  msg.set_key(key);
  msg.set_value(value);

  if (Message_DebugEnabled(__FILE__)) {
    std::stringstream ss;
    ss << "Sending: ";
    PrintBranch(branch, ss);
    Debug("%s", ss.str().c_str());
  }

  sclients[i]->Write(msg);
}

void Client::Commit(PendingRequest *req, const proto::Branch &branch) {
  proto::Prepare prepare;
  *prepare.mutable_branch() = branch;
  prepare.mutable_branch()->set_id(prepareBranchIds);
  prepareBranchIds++;
  req->sentPrepares++;
  for (auto shard : branch.shards()) {
    sclients[shard]->Prepare(prepare);
  }
}

void Client::Abort(const proto::Branch &branch) {
  proto::Abort abort;
  *abort.mutable_branch() = branch;
  for (auto shard : branch.shards()) {
    sclients[shard]->Abort(abort);
  }
}

void Client::ProcessPrepareKOs(PendingRequest *req, const proto::Branch &branch) {
  req->prepareResponses++;
  if (req->prepareResponses == req->sentPrepares) {
    Debug("Received responses for all outstanding prepares (%lu).", req->sentPrepares);
    
    Abort(branch);

    req->ecb(FAILED, std::map<std::string, std::string>());
    pendingReqs.erase(pendingReqs.find(req->id));
    delete req;
  } else {
    Debug("Prepare failed. Waiting on %lu other prepare responses (out of %lu).",
        req->sentPrepares - req->prepareResponses, req->sentPrepares);
  }
}

} // namespace mortystore
