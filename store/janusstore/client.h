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

#include "store/janusstore/transaction.h"
#include "store/janusstore/shardclient.h"
#include "store/janusstore/janus-proto.pb.h"

#include <thread>

namespace janusstore {

class Client : public ::Client {
public:
    Client(const std::string configPath, int nShards, int closestReplica,
        Transport *transport);
    virtual ~Client();

    // begins PreAccept phase
    void PreAccept(Transaction *txn, uint64_t ballot);

    // called from PreAcceptCallback when a fast quorum is not obtained
    void Accept(
        uint64_t txn_id,
        std::set<uint64_t> deps,
        uint64_t ballot);

    // different from the public Commit() function; this is a Janus commit
    void Commit(uint64_t txn_id, std::set<uint64_t> deps);

private:
    // Unique ID for this client.
    uint64_t client_id;

    // Number of shards.
    uint64_t nshards;

    // Transport used by IR client proxies.
    Transport *transport;

    // Current highest ballot
    // TODO should probably also pair this with client_id
    uint64_t ballot;

    // Ongoing transaction ID.
    // TODO figure out best way to pair this with client_id for unique t_id
    uint64_t txn_id;

    // Map of txn_id to aggregated list of txn_id dependencies
    std::unordered_map<uint64_t, std::set<uint64_t>> aggregated_deps;

    // List of participants in the ongoing transaction.
    std::set<int> participants;

    // List of replied shards for the current transaction
    std::set<int> responded;

    // Buffering client for each shard.
    std::vector<ShardClient *> bclient;

    void setParticipants(Transaction *txn);

/* * coordinator role (co-located with client but not visible to client) * */

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
};

} // namespace janusstore

#endif /* _JANUS_CLIENT_H_ */
