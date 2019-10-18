#include "store/mortystore/client.h"

#include "store/mortystore/common.h"

namespace mortystore {

Client::Client(const std::string configPath, uint64_t client_id, int nShards, int nGroups,
    int closestReplica, Transport *transport, partitioner part) : client_id(client_id), nshards(nShards),
    ngroups(nGroups), transport(transport), part(part), lastReqId(0UL),
    prepareBranchIds(0UL) {
  t_id = (client_id / 10000) * 10000;

  Debug("Initializing Morty client with id [%lu] %lu", client_id, nshards);

  /* Start a client for each shard. */
  for (uint64_t i = 0; i < nshards; i++) {
    std::string shardConfigPath = configPath + std::to_string(i) + ".config";
    sclients.push_back(new ShardClient(shardConfigPath, transport,
        client_id, i, closestReplica, this));
  }

  Debug("Morty client [%lu] created! %lu", client_id, nshards);
}

Client::~Client() {
  for (auto sclient : sclients) {
    delete sclient;
  }
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
  std::cerr << "Executing next: ";
  PrintBranch(branch);

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
      Commit(branch);
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
    ValueOnBranch(&branch->seq()[branch->seq().size() - 1], key, val);
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

  itr->second->prepareOKs[msg.branch().id()]++;
  if (itr->second->prepareOKs[msg.branch().id()] == msg.branch().shards().size()) {
    proto::Commit commit;
    *commit.mutable_branch() = msg.branch();
    for (auto shard : msg.branch().shards()) {
      sclients[shard]->Commit(commit);
    }
    ClientBranch clientBranch = GetClientBranch(msg.branch());
    itr->second->ecb(SUCCESS, clientBranch.readValues);
  } else if (itr->second->prepareOKs[msg.branch().id()] +
      itr->second->prepareKOes[msg.branch().id()].size() ==
      msg.branch().shards().size()) {
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

  itr->second->prepareKOes[msg.branch().id()].push_back(msg);
  if (itr->second->prepareOKs[msg.branch().id()] +
      itr->second->prepareKOes[msg.branch().id()].size() ==
      msg.branch().shards().size()) {
    // TODO deadlock resolution
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

  sclients[i]->Write(msg);
}

void Client::Commit(const proto::Branch &branch) {
  proto::Prepare prepare;
  *prepare.mutable_branch() = branch;
  prepare.mutable_branch()->set_id(prepareBranchIds);
  prepareBranchIds++;
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

} // namespace mortystore
