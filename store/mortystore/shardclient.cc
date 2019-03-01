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

  client = new replication::ir::IRClient(config, transport, client_id);

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

  // Potentially wait for any previous pending requests.
  //   maybe we only need to cleanup in SpecClient?
}

void ShardClient::Get(uint64_t id, const string &key, get_callback cb) {
  // Send the GET operation to appropriate shard.
  Debug("[shard %i] Sending GET [%lu : %s]", shard, id, key.c_str());

  // create request
  string request_str;
  proto::Request request;
  request.set_op(proto::Request::GET);
  request.set_txnid(id);
  request.mutable_get()->set_key(key);
  request.SerializeToString(&request_str);

  transport->Timer(0, [this, request_str, cb]() {
    client->InvokeUnlogged(replica, request_str, std::bind(
          &ShardClient::GetCallback, this, cb, std::placeholders::_1,
          std::placeholders::_2), std::bind(&ShardClient::GetTimeout, this),
        1000); // timeout in ms
                  // set to 1 second by default
  });
}

void ShardClient::Put(uint64_t id, const std::string &key,
    const std::string &value, put_callback cb) {
  // Send the PUT operation to appropriate shard.
  Debug("[shard %i] Sending PUT [%lu : %s]", shard, id, key.c_str());

  // create request
  string request_str;
  proto::Request request;
  request.set_op(proto::Request::PUT);
  request.set_txnid(id);
  request.mutable_put()->set_key(key);
  request.mutable_put()->set_value(value);
  request.SerializeToString(&request_str);

  transport->Timer(0, [this, request_str, cb]() {
    client->InvokeUnlogged(replica, request_str, std::bind(
          &ShardClient::PutCallback, this, cb, std::placeholders::_1,
          std::placeholders::_2), std::bind(&ShardClient::PutTimeout, this),
        1000);
  });
}

void ShardClient::Prepare(uint64_t id, const Transaction &txn) {
  Debug("[shard %i] Sending PREPARE [%lu]", shard, id);

  // create prepare request
  string request_str;
  proto::Request request;
  request.set_op(proto::Request::PREPARE);
  request.set_txnid(id);
  txn.serialize(request.mutable_prepare()->mutable_txn());
  request.SerializeToString(&request_str);

  transport->Timer(0, [this, request_str]() {
    client->InvokeUnlogged(replica, request_str,
        std::bind(&ShardClient::PrepareCallback, this, std::placeholders::_1,
          std::placeholders::_2), std::bind(&ShardClient::PrepareTimeout, this),
        1000);
  });
}

void ShardClient::Commit(uint64_t id, const Transaction &txn,
    commit_callback cb) {
  Debug("[shard %i] Sending COMMIT [%lu]", shard, id);

  // create commit request
  string request_str;
  proto::Request request;
  request.set_op(proto::Request::COMMIT);
  request.set_txnid(id);
  request.SerializeToString(&request_str);

  transport->Timer(0, [this, request_str, cb]() {
    client->InvokeUnlogged(replica, request_str,
        std::bind(&ShardClient::CommitCallback, this, cb, std::placeholders::_1,
          std::placeholders::_2), std::bind(&ShardClient::CommitTimeout, this),
        1000); // timeout in ms
  });
}

void ShardClient::Abort(uint64_t id, const Transaction &txn,
    abort_callback cb) {
  Debug("[shard %i] Sending ABORT [%lu]", shard, id);

  // create abort request
  string request_str;
  proto::Request request;
  request.set_op(proto::Request::ABORT);
  request.set_txnid(id);
  txn.serialize(request.mutable_abort()->mutable_txn());
  request.SerializeToString(&request_str);

  transport->Timer(0, [this, request_str, cb]() {
    client->InvokeUnlogged(replica, request_str,
        std::bind(&ShardClient::AbortCallback, this, cb, std::placeholders::_1,
          std::placeholders::_2), std::bind(&ShardClient::AbortTimeout, this),
        1000); // timeout in ms
  });

}

void ShardClient::GetTimeout() {
}

void ShardClient::PutTimeout() {
}

void ShardClient::PrepareTimeout() {
}

void ShardClient::CommitTimeout() {
}

void ShardClient::AbortTimeout() {
}


/* Callback from a shard replica on get operation completion. */
void ShardClient::GetCallback(get_callback cb, const std::string &request_str,
    const std::string &reply_str) {
  /* Replies back from a shard. */
  proto::Reply reply;
  reply.ParseFromString(reply_str);

  Debug("[shard %lu:%i] GET callback [%d]", client_id, shard, reply.status());
}

void ShardClient::PutCallback(put_callback cb, const std::string &request_str,
    const std::string &reply_str) {
  /* Replies back from a shard. */
  proto::Reply reply;
  reply.ParseFromString(reply_str);

  Debug("[shard %lu:%i] PUT callback [%d]", client_id, shard, reply.status());
}


/* Callback from a shard replica on prepare operation completion. */
void ShardClient::PrepareCallback(
    const std::string &request_str, const std::string &reply_str) {
  proto::Reply reply;

  reply.ParseFromString(reply_str);
  Debug("[shard %lu:%i] PREPARE callback [%d]", client_id, shard,
      reply.status());
}

/* Callback from a shard replica on commit operation completion. */
void ShardClient::CommitCallback(commit_callback cb,
    const std::string &request_str, const std::string &reply_str) {
  Debug("[shard %lu:%i] COMMIT callback", client_id, shard);
}

/* Callback from a shard replica on abort operation completion. */
void ShardClient::AbortCallback(abort_callback cb,
    const std::string &request_str, const std::string &reply_str) {
  Debug("[shard %lu:%i] ABORT callback", client_id, shard);
}

} // namespace mortystore
