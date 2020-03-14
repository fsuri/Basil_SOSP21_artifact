#include "store/mortystore/client.h"

#include <sstream>

#include "store/mortystore/common.h"

#define TXN_ID_SHIFT 20

namespace mortystore {

Client::Client(transport::Configuration *config, uint64_t client_id, int nShards,
    int nGroups, int closestReplica, Transport *transport, partitioner part,
    bool debugStats) : config(config), client_id(client_id), nshards(nShards),
    ngroups(nGroups), transport(transport), part(part), debugStats(debugStats),
    lastReqId(0UL), prepareBranchIds(0UL) {
  t_id = client_id << TXN_ID_SHIFT; 

  Debug("Initializing Morty client with id [%lu] %lu", client_id, nshards);

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

  if (debugStats) {
    if (clientBranch.opCount > 0) {
      uint64_t ns = Latency_End(&opLat);
      stats.Add("op" + std::to_string(clientBranch.opCount), ns);
    }
    Latency_Start(&opLat);
  }

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
      Get(req, branch, op.key);
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
      ValueOnBranch(branch, op.key(), val);
      clientBranch.readValues.insert(std::make_pair(op.key(), val));
    }
  }
  return clientBranch;
}

void Client::HandleReadReply(const TransportAddress &remote,
    const proto::ReadReply &msg, uint64_t shard) {
  auto itr = pendingReqs.find(msg.branch().txn().id());
  if (itr == pendingReqs.end()) {
    return;
  }

  itr->second->waitingToAbort = false;

  ClientBranch clientBranch = GetClientBranch(msg.branch());

  if (debugStats) {
    struct timeval now;
    gettimeofday(&now, NULL);
    uint64_t diff = now.tv_usec - msg.ts();
    stats.Add("recv_read_write_reply" + std::to_string(msg.branch().txn().id()),
        diff);
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

  itr->second->waitingToAbort = false;

  ClientBranch clientBranch = GetClientBranch(msg.branch());

  if (debugStats) {
    struct timeval now;
    gettimeofday(&now, NULL);
    uint64_t diff = now.tv_usec - msg.ts();
    stats.Add("recv_read_write_reply" + std::to_string(msg.branch().txn().id()),
        diff);
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
    if (debugStats) {
      uint64_t ns = Latency_End(&opLat);
      stats.Add("commit", ns);
    }

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
      static_cast<int64_t>(itr->second->prepareKOes[msg.branch()].size()) ==
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
      static_cast<int64_t>(itr->second->prepareKOes[msg.branch()].size()) ==
      msg.branch().shards().size()) {
    ProcessPrepareKOs(itr->second, msg.branch());
  }
}


void Client::Get(PendingRequest *req, proto::Branch &branch,
    const std::string &key) {
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

  struct timeval now;
  gettimeofday(&now, NULL);
  msg.set_ts(now.tv_usec);

  if (Message_DebugEnabled(__FILE__)) {
    std::stringstream ss;
    ss << "Sending: ";
    PrintBranch(branch, ss);
    Debug("%s", ss.str().c_str());
  }

  // Check if this branch already read this key
  bool alreadyRead = false;
  for (int64_t i = 0; i < branch.txn().ops_size() - 1; ++i) {
    if (branch.txn().ops(i).key() == key) {
      alreadyRead = true;
      break;
    }
  }

  RecordBranch(msg.branch());
  if (alreadyRead) {
    ExecuteNextOperation(req, branch);
  } else {
    sclients[i]->Read(msg);
  }
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

  struct timeval now;
  gettimeofday(&now, NULL);
  msg.set_ts(now.tv_usec);

  if (Message_DebugEnabled(__FILE__)) {
    std::stringstream ss;
    ss << "Sending: ";
    PrintBranch(branch, ss);
    Debug("%s", ss.str().c_str());
  }

  RecordBranch(msg.branch());
  sclients[i]->Write(msg);
}

void Client::Commit(PendingRequest *req, const proto::Branch &branch) {
  if (Message_DebugEnabled(__FILE__)) {
    std::stringstream ss;
    ss << "Sending: ";
    PrintBranch(branch, ss);
    Debug("%s", ss.str().c_str());
  }


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
  Debug("Aborting shards %d", branch.shards().size());
  for (auto shard : branch.shards()) {
    sclients[shard]->Abort(abort);
  }
}

void Client::ProcessPrepareKOs(PendingRequest *req,
    const proto::Branch &branch) {
  req->prepareResponses++;
  if (req->prepareResponses == req->sentPrepares) {
    req->waitingToAbort = true;
    uint64_t reqId = req->id;
    //transport->Timer(10000, [&,reqId](){
      auto itr = pendingReqs.find(reqId);
      if (itr == pendingReqs.end()) {
        return;
      }

      if (!itr->second->waitingToAbort) {
        return;
      }

      if (debugStats) {
        uint64_t ns = Latency_End(&opLat);
        stats.Add("abort", ns);
      }

      Debug("Received responses for all outstanding prepares (%lu).",
          itr->second->sentPrepares);
      
      Abort(branch);

      itr->second->ecb(FAILED, std::map<std::string, std::string>());
      pendingReqs.erase(pendingReqs.find(itr->second->id));
      delete itr->second;
    //});
  } else {
    Debug("Prepare failed. Waiting on %lu other prepare responses (out of %lu).",
        req->sentPrepares - req->prepareResponses, req->sentPrepares);
  }
}

void Client::RecordBranch(const proto::Branch &branch) {
  UW_ASSERT(sent_branches.find(branch) == sent_branches.end());    
  sent_branches.insert(branch);
}

} // namespace mortystore
