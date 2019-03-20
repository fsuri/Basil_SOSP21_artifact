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

class ShardClient : public TxnClient {
public:
    /* Constructor needs path to shard config. */
    ShardClient(const std::string &configPath, Transport *transport,
        uint64_t client_id, int shard, int closestReplica);
    virtual ~ShardClient();

    // Begin a transaction.
    virtual void Begin(uint64_t id);

    // Get the value corresponding to key.
    virtual void Get(uint64_t id, const std::string &key, get_callback gcb,
        get_timeout_callback gtcb, uint32_t timeout);

    // Set the value for the given key.
    virtual void Put(uint64_t id, const std::string &key,
        const std::string &value, put_callback pcb,
        put_timeout_callback ptcb, uint32_t timeout);

    // Commit all Get(s) and Put(s) since Begin().
    virtual void Commit(uint64_t id, const Transaction & txn,
        commit_callback ccb, commit_timeout_callback ctcb, uint32_t timeout);
  
    // Abort all Get(s) and Put(s) since Begin().
    virtual void Abort(uint64_t id, const Transaction &txn,
        abort_callback acb, abort_timeout_callback atcb, uint32_t timeout);

/* * coordinator role (co-located with client but not visible to client) * */

    // Initiate the PreAccept phase for this shard.
    // TODO check params
    virtual void PreAccept(uint64_t id, const Transaction &txn, uint64_t ballot, preaccept_callback pcb);

    // Initiate the Accept phase for this shard.
    // TODO check params
    virtual void Accept(uint64_t id, const Transaction &txn, std::vector<std::string> deps, uint64_t ballot, accept_callback acb);

    // Initiate the Commit phase for this shard.
    // TODO check params
    virtual void CommitJanusTxn(uint64_t id, const Transaction &txn, std::vector<std::string> deps, commit_txn_callback ctcb);

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
        uint64_t t_id, int status,
        std::unordered_map<uint64_t,std::vector<uint64_t>> deps);
    // 
    void AcceptCallback(uint64_t t_id);
    // TODO maybe change the type of [results]
    void CommitJanusTxnCallback(uint64_t t_id, std::vector<uint64_t> results);

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
