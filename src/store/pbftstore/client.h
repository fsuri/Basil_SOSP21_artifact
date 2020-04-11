#ifndef _PBFT_CLIENT_H_
#define _PBFT_CLIENT_H_

#include "lib/assert.h"
#include "lib/keymanager.h"
#include "lib/message.h"
#include "lib/configuration.h"
#include "lib/udptransport.h"
#include "replication/ir/client.h"
#include "store/common/timestamp.h"
#include "store/common/truetime.h"
#include "store/common/frontend/client.h"
#include "store/pbftstore/pbft-proto.pb.h"
#include "store/pbftstore/shardclient.h"

#include <unordered_map>

namespace pbftstore {

class Client : public ::Client {
 public:
  Client(const transport::Configuration& config, int nGroups, int nShards,
      Transport *transport, partitioner part,
      uint64_t readQuorumSize, bool signMessages,
      bool validateProofs, KeyManager *keyManager,
      TrueTime timeserver = TrueTime(0,0));
  ~Client();

  // Begin a transaction.
  void Begin();

  // Get the value corresponding to key.
  void Get(const std::string &key, get_callback gcb,
      get_timeout_callback gtcb, uint32_t timeout);

  // Set the value for the given key.
  void Put(const std::string &key, const std::string &value,
      put_callback pcb, put_timeout_callback ptcb,
      uint32_t timeout);

  // Commit all Get(s) and Put(s) since Begin().
  void Commit(commit_callback ccb, commit_timeout_callback ctcb,
      uint32_t timeout);

  // Abort all Get(s) and Put(s) since Begin().
  void Abort(abort_callback acb, abort_timeout_callback atcb,
      uint32_t timeout);

  std::vector<int> Stats();

 private:
  uint64_t client_id;
  /* Configuration State */
  transport::Configuration config;
  // Number of replica groups.
  uint64_t nshards;
  // Number of replica groups.
  uint64_t ngroups;
  // Transport used by shard clients.
  Transport *transport;
  // Client for each shard
  std::vector<ShardClient *> bclient;
  partitioner part;
  uint64_t readQuorumSize;
  bool signMessages;
  bool validateProofs;
  KeyManager *keyManager;
  // TrueTime server.
  TrueTime timeServer;


  /* Transaction Execution State */
  // Current transaction.
  proto::Transaction txn;
  bool txnInProgress;
  // collected decisions
  std::unordered_map<uint64_t, proto::GroupedDecisions> groupedDecision;
  std::unordered_map<uint64_t, proto::GroupedSignedDecisions> groupedSignedDecision;
  
  std::unordered_set<uint64_t> writebackAcks;

  /* Debug State */
  std::unordered_map<std::string, uint32_t> statInts;

  void WriteBack(const proto::ShardDecisions& dec, commit_callback ccb, commit_timeout_callback ctcb,
    uint32_t timeout);

  void WriteBackSigned(const proto::ShardSignedDecisions& dec, commit_callback ccb, commit_timeout_callback ctcb,
    uint32_t timeout);

  bool IsParticipant(int g);
};

} // namespace pbftstore

#endif /* _PBFT_CLIENT_H_ */
