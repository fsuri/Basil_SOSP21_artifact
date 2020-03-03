#ifndef MORTY_SERVER_H
#define MORTY_SERVER_H

#include "replication/common/replica.h"
#include "store/mortystore/morty-proto.pb.h"
#include "store/server.h"
#include "store/common/backend/txnstore.h"
#include "store/common/stats.h"
#include "store/mortystore/common.h"
#include "store/mortystore/perm_branch_generator.h"
#include "store/mortystore/lw_branch_generator.h"
#include "store/mortystore/specstore.h"

#include <unordered_map>

namespace mortystore {

typedef std::pair<const TransportAddress *, proto::Prepare> PrepareBatchItem;

class Server : public TransportReceiver, public ::Server {
 public:
  Server(const transport::Configuration &config, int groupIdx, int idx,
      Transport *transport, bool debugStats, uint64_t prepareBatchPeriod);
  virtual ~Server();

  virtual void ReceiveMessage(const TransportAddress &remote,
      const std::string &type, const std::string &data, void *meta_data) override;

  virtual void Load(const std::string &key, const std::string &value,
      const Timestamp timestamp) override;

  virtual inline Stats &GetStats() override { return stats; };

 private:
  /** State Machine Transitions **/
  void HandleRead(const TransportAddress &remote, const proto::Read &msg);
  void HandleWrite(const TransportAddress &remote, const proto::Write &msg);
  void HandlePrepare(const TransportAddress &remote, const proto::Prepare &msg);
  void HandleKO(const TransportAddress &remote, const proto::KO &msg);
  void HandleCommit(const TransportAddress &remote, const proto::Commit &msg);
  void HandleAbort(const TransportAddress &remote, const proto::Abort &msg);
  void PrepareBatchTrigger();
  /** End State Machine Transitions **/

  /** State Machine Helper Functions **/
  void SendBranchReplies(const proto::Branch &init, proto::OperationType type,
      const std::string &key);
  bool CheckBranch(const TransportAddress &addr, const proto::Branch &branch);
  bool IsStaleMessage(uint64_t txn_id) const;
  /** End State Machine Helper Functions **/

  const transport::Configuration &config;
  int groupIdx;
  int idx;
  Transport *transport;
  bool debugStats;
  Latency_t readWriteResp;
  Stats stats;

  /** State Machine Configuration **/
  uint64_t prepareBatchPeriod;
  /** End State Machine Configuration **/

  /** State Machine State Variables **/
  SpecStore store;
  std::vector<proto::Transaction> prepared;
  std::unordered_map<uint64_t, const TransportAddress *> txn_coordinators;
  std::vector<proto::Branch> waiting;
  std::unordered_map<uint64_t, const TransportAddress *> shards;
  std::set<uint64_t> committed_txn_ids;
  std::set<uint64_t> prepared_txn_ids;
  std::set<uint64_t> aborted_txn_ids;
  LWBranchGenerator generator;
  std::vector<PrepareBatchItem> prepareBatch;
  /** End State Machine State Variables **/

};

} // namespace mortystore

#endif /* MORTY_SERVER_H */

