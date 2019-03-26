#include "store/mortystore/shardclient.h"

namespace mortystore {

ShardClient::ShardClient(const string &configPath, Transport *transport,
    uint64_t client_id, int shard, int closestReplica) : client_id(client_id),
      transport(transport), shard(shard) {
  std::ifstream configStream(configPath);
  if (configStream.fail()) {
    Panic("Unable to read configuration file: %s\n", configPath.c_str());
  }

  transport::Configuration config(configStream);
  this->config = &config;

  client = new ReplicaClient(config, transport, client_id);

  if (closestReplica == -1) {
    // choose arbitrary replica as favored replica
    replica = client_id % config.n;
  } else {
    replica = closestReplica;
  }
  Debug("Sending unlogged to replica %i", replica);
}

ShardClient::~ShardClient() {
    delete client;
}

void ShardClient::Begin(uint64_t id) {
  Debug("[shard %i] BEGIN: %lu", shard, id);
}

void ShardClient::Get(uint64_t id, const std::string &key, get_callback gcb,
      get_timeout_callback gtcb, uint32_t timeout) {
  Debug("[shard %i] Sending GET [%lu : %s]", shard, id, key.c_str());

  std::string request_str;
  proto::Request request;
  request.set_op(proto::Request::GET);
  request.set_txnid(id);
  request.mutable_get()->set_key(key);
  request.SerializeToString(&request_str);

  uint64_t reqId = lastReqId++;
  PendingGet *pendingGet = new PendingGet(reqId);
  pendingGets[reqId] = pendingGet;
  pendingGet->key = key;
  pendingGet->gcb = gcb;
  pendingGet->gtcb = gtcb;

  AddOutstanding(id, reqId);

  client->InvokeUnlogged(replica, request_str, std::bind(
        &ShardClient::GetCallback, this, pendingGet->reqId,
        std::placeholders::_1, std::placeholders::_2),
      std::bind(&ShardClient::GetTimeout, this, pendingGet->reqId), timeout);
}

void ShardClient::Get(uint64_t id, const std::string &key,
    const Timestamp &timestamp, get_callback gcb, get_timeout_callback gtcb,
    uint32_t timeout) {
  Panic("Not implemented.");
}

void ShardClient::Put(uint64_t id, const std::string &key,
      const std::string &value, put_callback pcb, put_timeout_callback ptcb,
      uint32_t timeout) {
  Debug("[shard %i] Sending PUT [%lu : %s : %s]", shard, id, key.c_str(),
      value.c_str());
  
  std::string request_str;
  proto::Request request;
  request.set_op(proto::Request::PUT);
  request.set_txnid(id);
  request.mutable_put()->set_key(key);
  request.mutable_put()->set_value(value);
  request.SerializeToString(&request_str);

  uint64_t reqId = lastReqId++;
  PendingPut *pendingPut = new PendingPut(reqId);
  pendingPuts[reqId] = pendingPut;
  pendingPut->key = key;
  pendingPut->pcb = pcb;
  pendingPut->ptcb = ptcb;

  AddOutstanding(id, reqId);

  client->InvokeUnlogged(replica, request_str, std::bind(
        &ShardClient::PutCallback, this, pendingPut->reqId,
        std::placeholders::_1, std::placeholders::_2),
      std::bind(&ShardClient::PutTimeout, this, pendingPut->reqId), timeout);
}

void ShardClient::Prepare(uint64_t id, const Transaction &txn,
      const Timestamp &timestamp, prepare_callback pcb,
      prepare_timeout_callback ptcb, uint32_t timeout) {
  Debug("[shard %i] Sending PREPARE [%lu]", shard, id);
}

void ShardClient::Commit(uint64_t id, const Transaction & txn,
      uint64_t timestamp, commit_callback ccb, commit_timeout_callback ctcb,
      uint32_t timeout) {
  Debug("[shard %i] Sending COMMIT [%lu]", shard, id);
}  
  
void ShardClient::Abort(uint64_t id, const Transaction &txn,
      abort_callback acb, abort_timeout_callback atcb, uint32_t timeout) {
  Debug("[shard %i] Sending ABORT [%lu]", shard, id);
}

void ShardClient::MarkComplete(uint64_t tid) {
  client->MarkComplete();
  auto itr = outstanding.find(tid);
  ASSERT(itr != outstanding.end());
  for (auto reqId : itr->second) {
    if (pendingGets.find(reqId) != pendingGets.end()) {
      auto jtr = pendingGets.find(reqId);
      delete jtr->second;
      pendingGets.erase(jtr);
    } else if (pendingPuts.find(reqId) != pendingPuts.end()) {
      auto jtr = pendingPuts.find(reqId);
      delete jtr->second;
      pendingPuts.erase(jtr);
    } else if (pendingPrepares.find(reqId) != pendingPrepares.end()) {
      auto jtr = pendingPrepares.find(reqId);
      delete jtr->second;
      pendingPrepares.erase(jtr);
    } else if (pendingCommits.find(reqId) != pendingCommits.end()) {
      auto jtr = pendingCommits.find(reqId);
      delete jtr->second;
      pendingCommits.erase(jtr);
    } else if (pendingAborts.find(reqId) != pendingAborts.end()) {
      auto jtr = pendingAborts.find(reqId);
      delete jtr->second;
      pendingAborts.erase(jtr);
    }
  }
  outstanding.erase(itr);
}

void ShardClient::AddOutstanding(uint64_t tid, uint64_t reqId) {
  auto itr = outstanding.find(tid);
  if (itr == outstanding.end()) {
    std::vector<uint64_t> o(1);
    o.push_back(reqId);
    outstanding[tid] = o;
  } else {
    itr->second.push_back(reqId);
  }
}

/* Callback from a shard replica on get operation completion. */
void ShardClient::GetCallback(uint64_t reqId, const std::string &request_str,
    const std::string &reply_str) {
}

/* Callback from a shard replica on put operation completion. */
void ShardClient::PutCallback(uint64_t reqId, const std::string &request_str,
    const std::string &reply_str) {
}

/* Callback from a shard replica on prepare operation completion. */
void ShardClient::PrepareCallback(const std::string &request_str,
    const std::string &reply_str) {
}

/* Callback from a shard replica on commit operation completion. */
void ShardClient::CommitCallback(const std::string &request_str,
    const std::string &reply_str) {
}

/* Callback from a shard replica on abort operation completion. */
void ShardClient::AbortCallback(const std::string &request_str,
    const std::string &reply_str) {
}

void ShardClient::GetTimeout(uint64_t reqId) {
}

void ShardClient::PutTimeout(uint64_t reqId) {
}

void ShardClient::PrepareTimeout() {
}

void ShardClient::CommitTimeout() {
}

void ShardClient::AbortTimeout() {
}


} // namespace mortystore
