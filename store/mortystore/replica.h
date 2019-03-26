#ifndef MORTY_REPLICA_H
#define MORTY_REPLICA_H

#include "replication/common/replica.h"
#include "store/mortystore/morty-proto.pb.h"
#include "store/server.h"
#include "store/common/backend/txnstore.h"

namespace mortystore {

class SGStore {
 public:
  SGStore();
  SGStore(const SGStore &store);
  virtual ~SGStore();

  void Write(uint64_t tid, const std::string &key, const std::string &value);
  void Read(uint64_t tid, const std::string &key, std::string &value);

 private:
  struct SGValue {
    std::string v;
    uint64_t mrw;
    std::vector<uint64_t> mrr;
  };
  std::unordered_map<std::string, SGValue> values;
};

class Branch {
 private:
  SGStore store;
};

class Replica : public replication::Replica, public ::Server {
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

  void HandleGetMessage(const TransportAddress &remote, uint64_t tid,
      const proto::GetMessage &get);
  void HandlePutMessage(const TransportAddress &remote, uint64_t tid,
      const proto::PutMessage &put);

  void AddActiveTransaction(uint64_t tid);

  std::unordered_map<uint64_t, Transaction *> activeTxns;
  std::vector<uint64_t> activeTidsSorted;
  SGStore committedStore;

};

} // namespace mortystore

#endif /* MORTY_REPLICA_H */

