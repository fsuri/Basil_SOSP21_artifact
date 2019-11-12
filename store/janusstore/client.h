// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
#ifndef _JANUS_CLIENT_H_
#define _JANUS_CLIENT_H_

#include "lib/assert.h"
#include "lib/message.h"
#include "lib/configuration.h"
#include "lib/udptransport.h"
#include "replication/ir/client.h"
#include "store/common/frontend/client.h"
#include "store/common/frontend/bufferclient.h"
#include "store/common/frontend/async_client.h"
#include "store/common/frontend/one_shot_client.h"

#include "store/janusstore/transaction.h"
#include "store/janusstore/shardclient.h"
#include "store/janusstore/janus-proto.pb.h"

#include <thread>

namespace janusstore {

// callback for output commit
typedef std::function<void(uint64_t)> output_commit_callback;
typedef std::function<void()> completion_callback;

class Client : public OneShotClient {
public:
    Client(const std::string configPath, int nShards, int closestReplica, Transport *transport);
    Client(transport::Configuration *config, int nShards, int closestReplica, Transport *transport);
    ~Client();

    virtual void Execute(OneShotTransaction *txn, execute_callback ecb);

    // read
    void Read(string key);

    // begins PreAccept phase
    void PreAccept(Transaction *txn, uint64_t ballot, execute_callback ocb);

    // called from PreAcceptCallback when a fast quorum is not obtained
    void Accept(
        uint64_t txn_id,
        std::set<uint64_t> deps,
        uint64_t ballot);

    // different from the public Commit() function; this is a Janus commit
    void Commit(uint64_t txn_id, std::set<uint64_t> deps);

    struct PendingRequest {
        PendingRequest(uint64_t txn_id, execute_callback ccb)
        : txn_id(txn_id), ccb(ccb), has_fast_quorum(false), output_committed(false) {}
        ~PendingRequest() {}

        execute_callback ccb;
        uint64_t txn_id;
        bool output_committed;
        bool has_fast_quorum;
        std::set<uint64_t> aggregated_deps;
        std::set<uint64_t> participant_shards;
        std::set<uint64_t> responded_shards;
  };

// private: // make all fields public for testing
    // Unique ID for this client.
    uint64_t client_id;

    // Number of shards.
    uint64_t nshards;

    // Transport used by IR client proxies.
    Transport *transport;

    // Current highest ballot
    uint64_t ballot;

    // Next available transaction ID.
    uint64_t next_txn_id;

    // aggregated map to remove the above fields/maps up to aggregated_deps
    std::unordered_map<uint64_t, PendingRequest*> pendingReqs;

    // Buffering client for each shard.
    std::vector<ShardClient *> bclient;

    uint64_t keyToShard(string key, uint64_t nshards);

    void setParticipants(Transaction *txn);

/* * coordinator role (co-located with client but not visible to client) * */
    // read
    void ReadCallback(string key, string value);
    // callback when all participants for a shard have replied
    // shardclient aggregates all replies before invoking the callback
    // replies is a vector of all replies for this shard
    // deps is a map from replica ID to its deps for T (a list of other t_ids)
    void PreAcceptCallback(
        uint64_t txn_id, int shard, std::vector<janusstore::proto::Reply> replies);

    // callback when majority of replicas in each shard returns Accept-OK
    void AcceptCallback(uint64_t txn_id, int shard, std::vector<janusstore::proto::Reply> replies);

    // TODO maybe change the type of [results]
    void CommitCallback(uint64_t txn_id, int shard, std::vector<janusstore::proto::Reply> replies);
  transport::Configuration *config;
};

} // namespace janusstore

#endif /* _JANUS_CLIENT_H_ */
