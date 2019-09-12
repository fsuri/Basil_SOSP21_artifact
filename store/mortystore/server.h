#ifndef MORTY_REPLICA_H
#define MORTY_REPLICA_H

#include "replication/common/replica.h"
#include "store/mortystore/morty-proto.pb.h"
#include "store/server.h"
#include "store/common/backend/txnstore.h"

namespace mortystore {

class Replica : public TransportReceiver, public ::Server {
 public:
  Replica(const transport::Configuration &config, int idx,
      Transport *transport);
  virtual ~Replica();

  virtual void ReceiveMessage(const TransportAddress &remote,
      const std::string &type, const std::string &data) override;

  virtual void Load(const std::string &key, const std::string &value,
      const Timestamp timestamp) override;

 private:
  void HandleUnloggedRequest(const TransportAddress &remote,
      const proto::UnloggedRequestMessage &msg);

};

} // namespace mortystore

#endif /* MORTY_REPLICA_H */

