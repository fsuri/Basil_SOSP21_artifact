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
    ShardClient(transport::Configuration *config, Transport *transport,
        uint64_t client_id, int shard, int closestReplica);
    virtual ~ShardClient();

/* * coordinator role (co-located with client but not visible to client) * */

    // Initiate the PreAccept phase for this shard.
    virtual void PreAccept(const Transaction &txn, uint64_t ballot, client_preaccept_callback pcb);

    // Initiate the Accept phase for this shard.
    virtual void Accept(uint64_t txn_id, std::vector<uint64_t> deps, uint64_t ballot, client_accept_callback acb);

    // Initiate the Commit phase for this shard.
    virtual void Commit(uint64_t txn_id, std::vector<uint64_t> deps, client_commit_callback ccb);

// private: // made public for testing
    uint64_t client_id; // Unique ID for this client.
    Transport *transport; // Transport layer.
    transport::Configuration *config;
    int shard; // which shard this client accesses
    int num_replicas;
    int replica; // which replica to use for reads

    struct PendingRequest {
        PendingRequest(uint64_t txn_id)
        : txn_id(txn_id) {}
        ~PendingRequest() {}

        uint64_t txn_id;
        client_preaccept_callback cpcb;
        client_accept_callback cacb;
        client_commit_callback cccb;
        std::set<uint64_t> aggregated_deps;
        std::vector<janusstore::proto::Reply> preaccept_replies;
        std::vector<janusstore::proto::Reply> accept_replies;
        std::vector<janusstore::proto::Reply> commit_replies;
        uint64_t responded;
    };

    replication::ir::IRClient *client; // Client proxy.

    std::unordered_map<uint64_t, PendingRequest*> pendingReqs;

    // TODO will probably need to add fields for aggregating replica responses
    // Map of txn_id to aggregated list of txn_id dependencies for this shard
    std::unordered_map<uint64_t, std::set<uint64_t>> aggregated_deps;
    
    std::map<uint64_t, std::vector<janusstore::proto::Reply>> preaccept_replies;
    std::map<uint64_t, std::vector<janusstore::proto::Reply>> accept_replies;
    std::map<uint64_t, std::vector<janusstore::proto::Reply>> commit_replies;

    std::map<uint64_t, client_preaccept_callback> pcb_map;
    std::map<uint64_t, client_accept_callback> acb_map;
    std::map<uint64_t, client_commit_callback> ccb_map;


    /* Callbacks for hearing back from a shard for a Janus phase. */
    void PreAcceptCallback(uint64_t txn_id, janusstore::proto::Reply reply,
        client_preaccept_callback pcb);
    // 
    void AcceptCallback(uint64_t txn_id, janusstore::proto::Reply reply, client_accept_callback acb);
    void CommitCallback(uint64_t txn_id, janusstore::proto::Reply reply, client_commit_callback ccb);

    void PreAcceptContinuation(const string &request_str,
    const string &reply_str);
    void AcceptContinuation(const string &request_str,
    const string &reply_str);
    void CommitContinuation(const string &request_str,
    const string &reply_str);

    int responded;
    int txn_id;
};

} // namespace janusstore

#endif /* _JANUS_SHARDCLIENT_H_ */
