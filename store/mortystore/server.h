#ifndef MORTY_SERVER_H
#define MORTY_SERVER_H

#include "replication/common/replica.h"
#include "store/mortystore/morty-proto.pb.h"
#include "store/server.h"
#include "store/common/backend/txnstore.h"
#include "store/common/stats.h"
#include "store/mortystore/common.h"
#include "store/mortystore/branch_generator.h"
#include "store/mortystore/specstore.h"

#include <unordered_map>

namespace mortystore {

class Server : public TransportReceiver, public ::Server {
 public:
  Server(const transport::Configuration &config, int groupIdx, int idx,
      Transport *transport, bool debugStats);
  virtual ~Server();

  virtual void ReceiveMessage(const TransportAddress &remote,
      const std::string &type, const std::string &data, void *meta_data) override;

  virtual void Load(const std::string &key, const std::string &value,
      const Timestamp timestamp) override;

  virtual inline Stats &GetStats() override { return stats; };

 private:
  void HandleRead(const TransportAddress &remote, const proto::Read &msg);
  void HandleWrite(const TransportAddress &remote, const proto::Write &msg);
  void HandlePrepare(const TransportAddress &remote, const proto::Prepare &msg);
  void HandleKO(const TransportAddress &remote, const proto::KO &msg);
  void HandleCommit(const TransportAddress &remote, const proto::Commit &msg);
  void HandleAbort(const TransportAddress &remote, const proto::Abort &msg);

  void SendBranchReplies(const proto::Branch &init, proto::OperationType type,
      const std::string &key);
  bool CheckBranch(const TransportAddress &addr, const proto::Branch &branch);

  bool IsStaleMessage(uint64_t txn_id) const;

  const transport::Configuration &config;
  int groupIdx;
  int idx;
  Transport *transport;
  bool debugStats;
  SpecStore store;
  std::vector<proto::Transaction> committed;
  std::vector<proto::Transaction> prepared;
  std::unordered_map<uint64_t, const TransportAddress *> txn_coordinators;
  std::vector<proto::Branch> waiting;
  std::unordered_map<uint64_t, const TransportAddress *> shards;
  Stats stats;
  std::set<uint64_t> committed_txn_ids;
  std::set<uint64_t> prepared_txn_ids;
  std::set<uint64_t> aborted_txn_ids;
  BranchGenerator generator;
  Latency_t readWriteResp;

};

} // namespace mortystore

#endif /* MORTY_SERVER_H */

