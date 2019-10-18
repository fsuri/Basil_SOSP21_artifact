#ifndef _MORTY_SHARDCLIENT_H_
#define _MORTY_SHARDCLIENT_H_

#include "lib/assert.h"
#include "lib/configuration.h"
#include "lib/message.h"
#include "lib/transport.h"
#include "store/mortystore/morty-proto.pb.h"

#include <map>
#include <string>

namespace mortystore {

class Client;

class ShardClient : public TransportReceiver {
 public:
  /* Constructor needs path to shard config. */
  ShardClient(const std::string &configPath, Transport *transport,
      uint64_t client_id, int shard, int closestReplica, Client *client);
  virtual ~ShardClient();

  virtual void ReceiveMessage(const TransportAddress &remote,
      const std::string &type, const std::string &data);

  void Read(const proto::Read &read);
  void Write(const proto::Write &write);
  void Prepare(const proto::Prepare &prepare);
  void Commit(const proto::Commit &commit);
  void Abort(const proto::Abort &abort);

 private:
  uint64_t client_id; // Unique ID for this client.
  Client *client;
  Transport *transport; // Transport layer.
  transport::Configuration *config;
  int shard; // which shard this client accesses
  int replica; // which replica to use for reads

};

} // namespace mortystore

#endif /* _MORTY_SHARDCLIENT_H_ */
