// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
#ifndef _JANUS_SHARDCLIENT_H_
#define _JANUS_SHARDCLIENT_H_

#include "lib/assert.h"
#include "lib/configuration.h"
#include "lib/message.h"
#include "lib/transport.h"
#include "replication/ir/client.h"
#include "store/common/frontend/txnclient.h"

#include "store/janusstore/transaction.h"
#include "store/janusstore/janus-proto.pb.h"

#include <map>
#include <string>

namespace janusstore {

// client callbacks
typedef std::function<void(int, std::vector<janusstore::proto::Reply>)> client_preaccept_callback;
typedef std::function<void(int, std::vector<janusstore::proto::Reply>)> client_accept_callback;
typedef std::function<void(int, std::vector<janusstore::proto::Reply>)> client_commit_callback;


class ShardClient {
public:
    /* Constructor needs path to shard config. */
    ShardClient(const std::string &configPath, Transport *transport,
        uint64_t client_id, int shard, int closestReplica);
    virtual ~ShardClient();

/* * coordinator role (co-located with client but not visible to client) * */

    // Initiate the PreAccept phase for this shard.
    virtual void PreAccept(const Transaction &txn, uint64_t ballot, client_preaccept_callback pcb);

    // Initiate the Accept phase for this shard.
    virtual void Accept(uint64_t txn_id, std::vector<uint64_t> deps, uint64_t ballot, client_accept_callback acb);

    // Initiate the Commit phase for this shard.
    virtual void Commit(uint64_t txn_id, std::vector<uint64_t> deps, client_commit_callback ccb);

private:
    uint64_t client_id; // Unique ID for this client.
    Transport *transport; // Transport layer.
    transport::Configuration *config;
    int shard; // which shard this client accesses
    int replica; // which replica to use for reads
    int responded;

    replication::ir::IRClient *client; // Client proxy.

    // TODO will probably need to add fields for aggregating replica responses
    // Map of txn_id to aggregated list of txn_id dependencies for this shard
    std::unordered_map<uint64_t, std::set<uint64_t>> aggregated_deps;
    
    std::map<uint64_t, std::vector<janusstore::proto::Reply>> preaccept_replies;
    std::map<uint64_t, std::vector<janusstore::proto::Reply>> accept_replies;
    std::map<uint64_t, std::vector<janusstore::proto::Reply>> commit_replies;

    /* Callbacks for hearing back from a shard for a Janus phase. */
    void PreAcceptCallback(uint64_t txn_id, janusstore::proto::Reply reply,
        client_preaccept_callback pcb);
    // 
    void AcceptCallback(uint64_t txn_id, janusstore::proto::Reply reply, client_accept_callback acb);
    void CommitCallback(uint64_t txn_id, janusstore::proto::Reply reply, client_commit_callback ccb);

    void PreAcceptContinuation();
    void AcceptContinuation();
    void CommitContinuation();
};

} // namespace janusstore

#endif /* _JANUS_SHARDCLIENT_H_ */
