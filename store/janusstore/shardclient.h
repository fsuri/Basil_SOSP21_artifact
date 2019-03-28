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
// TODO define these
#include "store/janusstore/shardclient.h"
#include "store/janusstore/janus-proto.pb.h"
// end TODO

#include <map>
#include <string>

namespace janusstore {

typedef std::function<void(uint64_t, int, std::unordered_map<uint64_t,std::vector<uint64_t>>)> preaccept_callback;
typedef std::function<void(int)> accept_callback;
typedef std::function<void(int, std::vector<uint64_t>)> commit_txn_callback;

class ShardClient {
public:
    /* Constructor needs path to shard config. */
    ShardClient(const std::string &configPath, Transport *transport,
        uint64_t client_id, int shard, int closestReplica);
    virtual ~ShardClient();

/* * coordinator role (co-located with client but not visible to client) * */

    // Initiate the PreAccept phase for this shard.
    virtual void PreAccept(uint64_t id, const Transaction &txn, uint64_t ballot, preaccept_callback pcb);

    // Initiate the Accept phase for this shard.
    virtual void Accept(uint64_t txn_id, std::vector<std::string> deps, uint64_t ballot, accept_callback acb);

    // Initiate the Commit phase for this shard.
    virtual void Commit(uint64_t txn_id, std::vector<std::string> deps, commit_txn_callback ctcb);

private:
    uint64_t client_id; // Unique ID for this client.
    Transport *transport; // Transport layer.
    transport::Configuration *config;
    int shard; // which shard this client accesses
    int replica; // which replica to use for reads

    replication::ir::IRClient *client; // Client proxy.

    // TODO will probably need to add fields for aggregating replica responses

    /* Callbacks for hearing back from a shard for a Janus phase. */
    void PreAcceptCallback(
        uint64_t txn_id, int status,
        std::unordered_map<uint64_t,std::vector<uint64_t>> deps);
    // 
    void AcceptCallback(uint64_t txn_id);
    // TODO maybe change the type of [results]
    void CommitCallback(uint64_t txn_id, std::vector<uint64_t> results);

    /* Helper Functions for starting and finishing requests */
    // TODO check if we can use these
    void StartRequest();
    void WaitForResponse();
    void FinishRequest(const std::string &reply_str);
    void FinishRequest();
    int SendGet(const std::string &request_str);
};

} // namespace janusstore

#endif /* _JANUS_SHARDCLIENT_H_ */
