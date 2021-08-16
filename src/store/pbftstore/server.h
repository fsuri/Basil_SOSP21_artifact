/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fs435@cornell.edu>
 *                Matthew Burke <matthelb@cs.cornell.edu>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************************/
#ifndef _PBFT_SERVER_H_
#define _PBFT_SERVER_H_

#include <memory>
#include <map>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>

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
  Server(const transport::Configuration& config, KeyManager *keyManager, int groupIdx, int idx, int numShards,
    int numGroups, bool signMessages, bool validateProofs, uint64_t timeDelta, Partitioner *part,
    bool order_commit = false, bool validate_abort = false,
    TrueTime timeServer = TrueTime(0, 0));
  ~Server();

  std::vector<::google::protobuf::Message*> Execute(const std::string& type, const std::string& msg);
  ::google::protobuf::Message* HandleMessage(const std::string& type, const std::string& msg);

  void Load(const std::string &key, const std::string &value,
      const Timestamp timestamp);

  Stats &GetStats();

  Stats* mutableStats();

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
  Partitioner *part;
  TrueTime timeServer;

  //addtional knobs: 1) order commit, 2) validate abort
  bool order_commit;
  bool validate_abort;

  std::shared_mutex atomicMutex;

  struct ValueAndProof {
    std::string value;
    std::shared_ptr<proto::CommitProof> commitProof;
  };

  std::shared_ptr<proto::CommitProof> dummyProof;

  VersionedKVStore<Timestamp, ValueAndProof> commitStore;


  std::vector<::google::protobuf::Message*> HandleTransaction(const proto::Transaction& transaction);

  ::google::protobuf::Message* HandleRead(const proto::Read& read);

  ::google::protobuf::Message* HandleGroupedCommitDecision(const proto::GroupedDecision& gdecision);

  ::google::protobuf::Message* HandleGroupedAbortDecision(const proto::GroupedDecision& gdecision);

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

  std::unordered_map<std::string, proto::GroupedDecision> bufferedGDecs;
  std::unordered_set<std::string> abortedTxs;

  // return true if this key is owned by this shard
  inline bool IsKeyOwned(const std::string &key) const {
    std::vector<int> txnGroups;
    return static_cast<int>((*part)(key, numShards, groupIdx, txnGroups) % numGroups) == groupIdx;
  }
};

}

#endif
