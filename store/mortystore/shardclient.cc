#include "store/mortystore/shardclient.h"

#include "store/mortystore/client.h"

namespace mortystore {

ShardClient::ShardClient(transport::Configuration *config, Transport *transport,
    uint64_t client_id, int shard, int closestReplica, Client *client) : client_id(client_id), client(client),
      transport(transport), config(config), shard(shard) {
  transport->Register(this, *config, -1, -1);

  if (closestReplica == -1) {
    replica = client_id % config->n;
  } else {
    replica = closestReplica;
  }
}

ShardClient::~ShardClient() {
}

void ShardClient::ReceiveMessage(const TransportAddress &remote,
      const std::string &type, const std::string &data, void *meta_data) {
  proto::ReadReply readReply;
  proto::WriteReply writeReply;
  proto::PrepareOK prepareOK;
  proto::CommitReply commitReply;
  proto::PrepareKO prepareKO;

  if (type == readReply.GetTypeName()) {
    readReply.ParseFromString(data);
    client->HandleReadReply(remote, readReply, shard);
  } else if (type == writeReply.GetTypeName()) {
    writeReply.ParseFromString(data);
    client->HandleWriteReply(remote, writeReply, shard);
  } else if (type == prepareOK.GetTypeName()) {
    prepareOK.ParseFromString(data);
    client->HandlePrepareOK(remote, prepareOK, shard);
  } else if (type == commitReply.GetTypeName()) {
    commitReply.ParseFromString(data);
    client->HandleCommitReply(remote, commitReply, shard);
  } else if (type == prepareKO.GetTypeName()) {
    prepareKO.ParseFromString(data);
    client->HandlePrepareKO(remote, prepareKO, shard);
  } else {
    Panic("Received unexpected message type: %s", type.c_str());
  }
}

void ShardClient::Read(const proto::Read &read) {
  transport->SendMessageToReplica(this, replica, read);
}

void ShardClient::Write(const proto::Write &write) {
  transport->SendMessageToReplica(this, replica, write);
}

void ShardClient::Prepare(const proto::Prepare &prepare) {
  transport->SendMessageToAll(this, prepare);
}

void ShardClient::Commit(const proto::Commit &commit) {
  transport->SendMessageToAll(this, commit);
}

void ShardClient::Abort(const proto::Abort &abort) {
  transport->SendMessageToAll(this, abort);
}

} // namespace morty
