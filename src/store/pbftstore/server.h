#ifndef _PBFT_SERVER_H_
#define _PBFT_SERVER_H_

#include <memory>
#include <map>
#include <unordered_map>

#include "store/pbftstore/app.h"
#include "store/pbftstore/server-proto.pb.h"
#include "store/server.h"
#include "lib/keymanager.h"
#include "lib/configuration.h"
#include "store/common/backend/versionstore.h"
#include "store/common/partitioner.h"
#include "store/common/truetime.h"

namespace pbftstore {

class Server : public App, public ::Server {
public:
  Server(const transport::Configuration& config, KeyManager *keyManager, int groupIdx, int idx, int numShards, int numGroups, bool signMessages, bool validateProofs, uint64_t timeDelta, partitioner part, TrueTime timeServer = TrueTime(0, 0));
  ~Server();

  ::google::protobuf::Message* Execute(const std::string& type, const std::string& msg);
  ::google::protobuf::Message* HandleMessage(const std::string& type, const std::string& msg);

  void Load(const std::string &key, const std::string &value,
      const Timestamp timestamp);

  Stats &GetStats();

private:
  Stats stats;
  transport::Configuration config;
  KeyManager* keyManager;
  int groupIdx;
  int idx;
  int id;
  int numShards;
  int numGroups;
  bool signMessages;
  bool validateProofs;
  uint64_t timeDelta;
  partitioner part;
  TrueTime timeServer;

  struct ValueAndProof {
    std::string value;
    std::shared_ptr<proto::CommitProof> commitProof;
  };

  VersionedKVStore<Timestamp, ValueAndProof> commitStore;


  ::google::protobuf::Message* HandleTransaction(const proto::Transaction& transaction);

  ::google::protobuf::Message* HandleRead(const proto::Read& read);

  ::google::protobuf::Message* HandleGroupedDecision(const proto::GroupedDecision& gdecision);

  ::google::protobuf::Message* returnMessage(::google::protobuf::Message* msg);

  // map from tx digest to transaction
  std::unordered_map<std::string, proto::Transaction> pendingTransactions;
  // map from key to ordered map of prepared tx timestamps to read timestamps
  std::unordered_map<std::string, std::map<Timestamp, Timestamp>> preparedReads;
  // map from key to ordered set of prepared transaction timestamps that write the key
  std::unordered_map<std::string, std::set<Timestamp>> preparedWrites;

  // map from key to ordered map of committed timestamps to read timestamp
  // so if a transaction with timestamp 5 reads version 3 of key A, we have A -> 5 -> 3
  // we wont have key collisions for the map because there each transaction has at
  // most 1 read for a key
  std::unordered_map<std::string, std::map<Timestamp, Timestamp>> committedReads;

  bool CCC(const proto::Transaction& txn);
  bool CCC2(const proto::Transaction& txn);

  void cleanupPendingTx(std::string digest);

  // return true if this key is owned by this shard
  inline bool IsKeyOwned(const std::string &key) const {
    return static_cast<int>(part(key, numShards) % numGroups) == groupIdx;
  }
};

}

#endif
