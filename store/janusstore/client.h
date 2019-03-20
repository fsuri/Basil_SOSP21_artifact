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
// TODO define these
#include "store/janusstore/shardclient.h"
#include "store/janusstore/janus-proto.pb.h"
// end TODO

#include <thread>

namespace janusstore {

class Client : public ::Client {
public:
    Client(const std::string configPath, int nShards, int closestReplica,
        Transport *transport);
    virtual ~Client();

    // Begin a transaction.
    virtual void Begin();

    // Get the value corresponding to key.
    virtual void Get(const std::string &key, get_callback gcb,
        get_timeout_callback gtcb, uint32_t timeout = GET_TIMEOUT);

    // Set the value for the given key.
    virtual void Put(const std::string &key, const std::string &value,
        put_callback pcb, put_timeout_callback ptcb,
        uint32_t timeout = PUT_TIMEOUT);

    // Commit all Get(s) and Put(s) since Begin().
    virtual void Commit(commit_callback ccb, commit_timeout_callback ctcb,
        uint32_t timeout);
  
    // Abort all Get(s) and Put(s) since Begin().
    virtual void Abort(abort_callback acb, abort_timeout_callback atcb,
        uint32_t timeout);

    virtual std::vector<int> Stats();

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
    uint64_t t_id;

    // Aggregated dependencies for the current transaction
    std::vector<uint64_t>> deps;

    // List of participants in the ongoing transaction.
    std::set<int> participants;

    // Buffering client for each shard.
    std::vector<BufferClient *> bclient;

/* * coordinator role (co-located with client but not visible to client) * */

    // begins PreAccept phase
    void PreAccept(Transaction txn, uint64_t ballot);

    // callback when all relevant replicas have replied
    // shardclient aggregates these replies into a status variable to indicate the result of PreAccept phase
    // deps is a map from replica ID to its deps for T (a list of other t_ids)
    void PreAcceptCallback(
        uint64_t t_id, int status,
        std::unordered_map<uint64_t,std::vector<uint64_t>> deps);

    // called from PreAcceptCallback when a fast quorum is not obtained
    void Accept(
        uint64_t t_id,
        std::vector<std::string> deps,
        uint64_t ballot);

    // callback when majority of replicas in each shard returns Accept-OK
    void AcceptCallback(uint64_t t_id);

    // different from the public Commit() function; this is a Janus commit
    void CommitJanusTxn(uint64_t t_id, std::vector<uint64_t>> deps);
    // TODO maybe change the type of [results]
    void CommitJanusTxnCallback(uint64_t t_id, std::vector<uint64_t> results);
};

} // namespace janusstore

#endif /* _JANUS_CLIENT_H_ */
