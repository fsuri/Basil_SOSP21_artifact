#include "store/mortystore/client.h"

namespace mortystore {

Client::Client(const std::string configPath, int nShards, int nGroups,
    int closestReplica, Transport *transport, partitioner part) : nshards(nShards),
    ngroups(nGroups), transport(transport), part(part), lastReqId(0UL) {
  // Initialize all state here;
  client_id = 0;
  while (client_id == 0) {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    client_id = dis(gen);
  }
  t_id = (client_id / 10000) * 10000;

  Debug("Initializing Morty client with id [%lu] %lu", client_id, nshards);

  sclients.reserve(nshards);
  /* Start a client for each shard. */
  for (uint64_t i = 0; i < nshards; i++) {
    std::string shardConfigPath = configPath + std::to_string(i) + ".config";
    sclients[i] = new ShardClient(shardConfigPath, transport,
        client_id, i, closestReplica);
  }

  Debug("Morty client [%lu] created! %lu", client_id, nshards);
}

Client::~Client() {
  for (auto sclient : sclients) {
    delete sclient;
  }
}

void Client::Execute(AsyncTransaction *txn, execute_callback ecb) {
  proto::Branch branch;
  ExecuteNextOperation(txn, branch);
}

void Client::ReceiveMessage(const TransportAddress &remote,
      const std::string &type, const std::string &data) {
  proto::ReadReply readReply;
  proto::WriteReply writeReply;
  proto::PrepareOK prepareOK;
  proto::CommitReply commitReply;

  if (type == readReply.GetTypeName()) {
    readReply.ParseFromString(data);
    HandleReadReply(remote, readReply);
  } else if (type == writeReply.GetTypeName()) {
    writeReply.ParseFromString(data);
    HandleWriteReply(remote, writeReply);
  } else if (type == prepareOK.GetTypeName()) {
    prepareOK.ParseFromString(data);
    HandlePrepareOK(remote, prepareOK);
  } else if (type == commitReply.GetTypeName()) {
    commitReply.ParseFromString(data);
    HandleCommitReply(remote, commitReply);
  } else {
    Panic("Received unexpected message type: %s", type.c_str());
  }
}

void Client::ExecuteNextOperation(AsyncTransaction *txn, proto::Branch &branch) {
  ClientBranch* clientBranch = GetClientBranch(branch);
  Operation op = currTxn->GetNextOperation(clientBranch->opCount,
      clientBranch->readValues);
  delete clientBranch;
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
      currEcb(false, std::map<std::string, std::string>());
      break;
    }
    default:
      NOT_REACHABLE();
  }
}

ClientBranch *Client::GetClientBranch(const proto::Branch &branch) {
  ClientBranch *clientBranch = new ClientBranch();
  clientBranch->opCount = branch.txn().ops_size();
  for (auto op : branch.txn().ops()) {
    if (op.type() == proto::OperationType::READ) {
      std::string val;
      ValueOnBranch(&branch, op.key(), val);
      clientBranch->readValues.insert(std::make_pair(op.key(), val));
    }
  }
  return clientBranch;
}

void Client::ValueOnBranch(const proto::Branch *branch, const std::string &key,
    std::string &val) {
  if (ValueInTransaction(branch->txn(), key, val)) {
    return;
  }
  ValueOnBranch(&branch->seq()[branch->seq().size() - 1], key, val);
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
    const proto::ReadReply &msg) {
  auto itr = pendingReqs.find(msg.branch().txn().id());
  if (itr == pendingReqs.end()) {
    return;
  }

  proto::Branch branch(msg.branch());
  ExecuteNextOperation(itr->second->txn, branch);
}

void Client::HandleWriteReply(const TransportAddress &remote,
    const proto::WriteReply &msg) {
  auto itr = pendingReqs.find(msg.branch().txn().id());
  if (itr == pendingReqs.end()) {
    return;
  }

  proto::Branch branch(msg.branch());
  ExecuteNextOperation(itr->second->txn, branch);
}

void Client::HandlePrepareOK(const TransportAddress &remote,
    const proto::PrepareOK &msg) {
  auto itr = pendingReqs.find(msg.branch().txn().id());
  if (itr == pendingReqs.end()) {
    return;
  }

}

void Client::HandleCommitReply(const TransportAddress &remote,
    const proto::CommitReply &msg) {
}

void Client::Get(proto::Branch &branch, const std::string &key) {
  // Contact the appropriate shard to get the value.
  int i = part(key, nshards) % ngroups;
  if (std::find(branch.shards().begin(), branch.shards().end(),
        i) != branch.shards().end()) {
    branch.mutable_shards()->Add(i);
  }
  proto::Operation *op = branch.mutable_txn()->add_ops();
  op->set_type(proto::OperationType::READ);
  op->set_key(key);

  proto::Read msg;
  *msg.mutable_branch() = branch;
  msg.set_key(key);

  sclients[i]->Read(msg, this);
}

void Client::Put(proto::Branch &branch, const std::string &key,
    const std::string &value) {
  // Contact the appropriate shard to get the value.
  int i = part(key, nshards) % ngroups;
  if (std::find(branch.shards().begin(), branch.shards().end(),
        i) != branch.shards().end()) {
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

  sclients[i]->Write(msg, this);
}

void Client::Commit(const proto::Branch &branch) {
  proto::Prepare prepare;
  *prepare.mutable_branch() = branch;
  for (auto shard : branch.shards()) {
    sclients[shard]->Prepare(prepare, this);
  }
}

void Client::Abort(const proto::Branch &branch) {
  proto::Abort abort;
  *abort.mutable_branch() = branch;
  for (auto shard : branch.shards()) {
    sclients[shard]->Abort(abort, this);
  }
}

} // namespace mortystore
