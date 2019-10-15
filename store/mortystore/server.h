#ifndef MORTY_SERVER_H
#define MORTY_SERVER_H

#include "replication/common/replica.h"
#include "store/mortystore/morty-proto.pb.h"
#include "store/server.h"
#include "store/common/backend/txnstore.h"
#include "store/common/stats.h"

#include <unordered_map>

namespace mortystore {

class Server : public TransportReceiver, public ::Server {
 public:
  Server(const transport::Configuration &config, int idx,
      Transport *transport);
  virtual ~Server();

  virtual void ReceiveMessage(const TransportAddress &remote,
      const std::string &type, const std::string &data) override;

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
  void GenerateBranches(const proto::Branch &init,
      proto::OperationType type, const std::string &key,
      std::vector<proto::Branch> &new_branches);
  void GenerateBranchesSubsets(const std::vector<proto::Branch> &pending_branches,
      const std::vector<uint64_t> &txns, std::vector<proto::Branch> &new_branches,
      std::vector<uint64_t> subset = std::vector<uint64_t>(),
      int64_t i = -1);
  void GenerateBranchesPermutations(const std::vector<proto::Branch> &pending_branches,
      const std::vector<uint64_t> &subset, std::vector<proto::Branch> &new_branches);
  bool CommitCompatible(const proto::Branch &branch, const std::vector<proto::Branch> &seq);
  bool WaitCompatible(const proto::Branch &branch, const std::vector<proto::Branch> &seq);
  bool ValidSubsequence(const proto::Transaction &txn,
      const std::vector<proto::Branch> &seq1,
      const std::vector<proto::Branch> &seq2);
  bool NoConflicts(const proto::Transaction &txn,
      const std::vector<proto::Branch> &seq);
  bool TransactionsConflict(const proto::Transaction &txn1,
      const proto::Transaction &txn2);
  void ValueOnBranch(const proto::Branch &branch, const std::string &key,
      std::string &val);
  bool ValueInTransaction(const proto::Transaction &txn, const std::string &key,
      std::string &val);
  bool CheckBranch(const TransportAddress &addr, const proto::Branch &branch);

  const transport::Configuration &config;
  int idx;
  Transport *transport;
  std::unordered_map<std::string, std::vector<proto::Branch>> pending_reads;
  std::unordered_map<std::string, std::vector<proto::Branch>> pending_writes;
  std::vector<proto::Branch> committed;
  std::vector<proto::Branch> prepared;
  std::unordered_map<uint64_t, const TransportAddress *> txn_coordinators;
  std::vector<proto::Branch> waiting;
  std::unordered_map<uint64_t, const TransportAddress *> shards;
  Stats stats;

};

} // namespace mortystore

#endif /* MORTY_SERVER_H */

