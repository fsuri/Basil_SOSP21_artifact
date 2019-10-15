#include "store/mortystore/shardclient.h"


#include "store/benchmark/async/tpcc/tpcc-proto.pb.h"

namespace mortystore {

using namespace std;
using namespace proto;

ShardClient::ShardClient(const string &configPath, Transport *transport,
    uint64_t client_id, int shard, int closestReplica, TransportReceiver *receiver) : client_id(client_id),
      transport(transport), shard(shard) {
  ifstream configStream(configPath);
  if (configStream.fail()) {
    Panic("Unable to read configuration file: %s\n", configPath.c_str());
  }

  this->config = new transport::Configuration(configStream);
  transport->Register(receiver, *config, -1);

  if (closestReplica == -1) {
    replica = client_id % config->n;
  } else {
    replica = closestReplica;
  }
  Debug("Sending unlogged to replica %i", replica);
}

ShardClient::~ShardClient() {
  delete config;
}

void ShardClient::Read(const proto::Read &read,
    TransportReceiver *receiver) {
  transport->SendMessageToReplica(receiver, replica, read);
}

void ShardClient::Write(const proto::Write &write,
    TransportReceiver *receiver) {
  transport->SendMessageToReplica(receiver, replica, write);
}

void ShardClient::Prepare(const proto::Prepare &prepare,
    TransportReceiver *receiver) {
  transport->SendMessageToAll(receiver, prepare);
}

void ShardClient::Commit(const proto::Commit &commit,
    TransportReceiver *receiver) {
  transport->SendMessageToAll(receiver, commit);
}

void ShardClient::Abort(const proto::Abort &abort,
    TransportReceiver *receiver) {
  transport->SendMessageToAll(receiver, abort);
}

} // namespace morty
