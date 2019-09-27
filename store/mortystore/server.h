#ifndef MORTY_SERVER_H
#define MORTY_SERVER_H

#include "replication/common/replica.h"
#include "store/mortystore/morty-proto.pb.h"
#include "store/server.h"
#include "store/common/backend/txnstore.h"

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

 private:
  void HandleRead(const TransportAddress &remote, const proto::Read &msg);
  void HandleWrite(const TransportAddress &remote, const proto::Write &msg);
  void HandlePrepare(const TransportAddress &remote, const proto::Prepare &msg);
  void HandleKO(const TransportAddress &remote, const proto::KO &msg);
  void HandleCommit(const TransportAddress &remote, const proto::Commit &msg);
  void HandleAbort(const TransportAddress &remote, const proto::Abort &msg);

  void CacheBranch(const proto::Branch &branch);
  void SendBranchReplies(proto::OperationType type,
      const std::string &key);
  void GenerateBranches(proto::OperationType type, const std::string &key,
      std::vector<proto::Branch *> new_branches);
  void GenerateBranchesSubsets(const std::unordered_set<uint64_t> &pending_branches,
      const std::vector<uint64_t> &txns, std::vector<proto::Branch *> new_branches,
      std::vector<uint64_t> subset = std::vector<uint64_t>(),
      size_t i = -1);
  void GenerateBranchesPermutations(const std::unordered_set<uint64_t> &pending_branches,
      const std::vector<uint64_t> &subset, std::vector<proto::Branch *> new_branches);
  bool CommitCompatible(uint64_t branch, const std::vector<uint64_t> &seq);
  bool WaitCompatible(uint64_t branch, const std::vector<uint64_t> &seq);
  bool ValidSubsequence(const proto::Transaction &txn,
      const std::vector<uint64_t> &seq1,
      const std::vector<uint64_t> &seq2);
  bool NoConflicts(const proto::Transaction &txn,
      const std::vector<uint64_t> &seq);
  bool TransactionsConflict(const proto::Transaction &txn1,
      const proto::Transaction &txn2);
  void ValueOnBranch(const proto::Branch *branch, const std::string &key,
      std::string &val);
  bool ValueInTransaction(const proto::Transaction &txn, const std::string &key,
      std::string &val);
  bool CheckBranch(const TransportAddress &addr, uint64_t branch);

  const transport::Configuration &config;
  int idx;
  Transport *transport;
  std::unordered_map<uint64_t, proto::Branch*> branches;
  std::unordered_map<std::string, std::unordered_set<uint64_t>> pending_reads;
  std::unordered_map<std::string, std::unordered_set<uint64_t>> pending_writes;
  std::vector<uint64_t> committed;
  std::vector<uint64_t> prepared;
  std::unordered_map<uint64_t, TransportAddress*> txn_coordinators;
  std::unordered_set<uint64_t> waiting;
  std::unordered_map<uint64_t, TransportAddress*> shards;

};

} // namespace mortystore

#endif /* MORTY_SERVER_H */

