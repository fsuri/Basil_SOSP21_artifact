// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
#include "store/janusstore/server.h"
#include "lib/udptransport.h"
#include <stack>

namespace janusstore {

using namespace std;
using namespace proto;

vector<uint64_t> _checkIfAllCommitting(unordered_map<uint64_t, Transaction> id_txn_map, vector<uint64_t> deps);

Server::Server(transport::Configuration config, int myIdx, Transport *transport)
    : config(config), myIdx(myIdx), transport(transport)
{
    transport::ReplicaAddress ra = config.replica(myIdx);
    server_id = ra.host + ra.port;
    store = new Store();
    transport->Register(this, config, myIdx);
}

Server::~Server() {
    delete store;
}

void Server::ReceiveMessage(const TransportAddress &remote,
                        const std::string &type, const std::string &data) {

    Request request;
    Reply reply;

    if (type == request.GetTypeName()) {
        request.ParseFromString(data);
        switch(request.op()) {
            case Request::PREACCEPT: {
                HandlePreAccept(remote, request.preaccept());
                break;
            }
            case Request::ACCEPT: {
                HandleAccept(remote, request.accept());
                break;
            }
            case Request::COMMIT: {
                HandleCommit(remote, request.commit());
                break;
            }
            case Request::INQUIRE: {
                break;
            }
            default: {
                Panic("Unrecognized request.");
                break;
            }
        }
    } else if (type == request.GetTypeName()) {
        switch(request.op()) {
            default: {
                Panic("Unrecognized reply.");
                break;
            }
        }
    } else {
        Panic("Unrecognized message.");
    }
}

void
Server::HandlePreAccept(const TransportAddress &remote,
                        const PreAcceptMessage &pa_msg)
{
    Reply reply;

    TransactionMessage txnMsg = pa_msg.txn();
    uint64_t txn_id = txnMsg.txnid();
    string server_id_txn = txnMsg.serverid();
    uint64_t ballot = pa_msg.ballot();

    // construct the transaction object
    Transaction txn = Transaction(txn_id, server_id_txn);
    for (int i = 0; i < txnMsg.gets_size(); i++) {
        string key = txnMsg.gets(i).key();
        txn.addReadSet(key);
    }

    for (int i = 0; i < txnMsg.puts_size(); i++) {
        PutMessage put = txnMsg.puts(i);
        txn.addWriteSet(put.key(), put.value());
    }

    vector<uint64_t> dep_list = BuildDepList(txn, ballot);

    // TODO less hacky way
    if (!dep_list.empty() && dep_list.at(0) == -1) {
        // return not ok
        PreAcceptNotOKMessage pa_not_ok_msg;

        reply.set_op(Reply::PREACCEPT_NOT_OK);
        reply.set_allocated_preaccept_not_ok(&pa_not_ok_msg);
        transport->SendMessage(this, remote, reply);
        return;
    }

    // create dep list for reply
    DependencyList dep;
    for (int i = 0; i < dep_list.size(); i++) {
        dep.add_txnid(dep_list[i]);
    }
    PreAcceptOKMessage preaccept_ok_msg;
    preaccept_ok_msg.set_txnid(txn.getTransactionId());
    preaccept_ok_msg.set_allocated_dep(&dep);

    reply.set_op(Reply::PREACCEPT_OK);
    reply.set_allocated_preaccept_ok(&preaccept_ok_msg);

    transport->SendMessage(this, remote, reply);
}

vector<uint64_t> Server::BuildDepList(Transaction txn, uint64_t ballot) {
    uint64_t txn_id = txn.getTransactionId();
    if (accepted_ballots.find(txn_id) != accepted_ballots.end() &&
     ballot > accepted_ballots[txn_id]) {
        // TODO less hacky way
        return vector<uint64_t>(-1);
    }
    accepted_ballots[txn_id] = ballot;

    txn.setTransactionStatus(TransactionMessage::PREACCEPT);
    id_txn_map[txn.getTransactionId()] = txn;

    // construct conflicts and read/write sets
    vector<uint64_t> dep_list;
    for (auto key : txn.getReadSet()) {
        if (read_key_txn_map.find(key) == read_key_txn_map.end()) {
          read_key_txn_map[key] = vector<uint64_t>(txn_id);
        } else {
          read_key_txn_map[key].push_back(txn_id);
        }

        // append conflicts
        if (write_key_txn_map.find(key) != write_key_txn_map.end()) {
            vector<uint64_t> other_txn_ids = write_key_txn_map[key];
            dep_list.insert(dep_list.end(), other_txn_ids.begin(), other_txn_ids.end());
        }
    }

    for (auto const& kv : txn.getWriteSet()) {
        string key = kv.first;
        if (write_key_txn_map.find(key) == write_key_txn_map.end()) {
          write_key_txn_map[key] = vector<uint64_t>(txn_id);
        } else {
          // append conflicts
          vector<uint64_t> other_txn_ids = write_key_txn_map[key];
          dep_list.insert(dep_list.end(), other_txn_ids.begin(), other_txn_ids.end());
          write_key_txn_map[key].push_back(txn_id);
        }

        if (read_key_txn_map.find(key) != read_key_txn_map.end()) {
          // append conflicts
          vector<uint64_t> other_txn_ids = read_key_txn_map[key];
          dep_list.insert(dep_list.end(), other_txn_ids.begin(), other_txn_ids.end());
        }
    }

    // add to dependency graph
    dep_map[txn_id] = dep_list;
    return dep_list;
}

void Server::HandleAccept(const TransportAddress &remote,
                          const proto::AcceptMessage &a_msg)
{
    Reply reply;

    uint64_t ballot = a_msg.ballot();
    uint64_t txn_id = a_msg.txnid();
    Transaction txn = id_txn_map[txn_id];

    // reconstruct dep_list from message
    vector<uint64_t> msg_deps;
    DependencyList received_dep = a_msg.dep();
    for (int i = 0; i < received_dep.txnid_size(); i++) {
        msg_deps.push_back(received_dep.txnid(i));
    }
    uint64_t accepted_ballot = accepted_ballots[txn_id];
    if (id_txn_map[txn_id].getTransactionStatus() == TransactionMessage::COMMIT || ballot < accepted_ballot) {
        // send back txn id and highest ballot for that txn
        AcceptNotOKMessage accept_not_ok_msg;
        accept_not_ok_msg.set_txnid(txn_id);
        accept_not_ok_msg.set_highest_ballot(accepted_ballot);

        reply.set_op(Reply::ACCEPT_NOT_OK);
        reply.set_allocated_accept_not_ok(&accept_not_ok_msg);
    } else {
        // replace dep_map with the list from the message
        dep_map[txn_id] = msg_deps;

        // update highest ballot with passed in ballot
        accepted_ballots[txn_id] = ballot;

        // update txn status to accept
        txn.setTransactionStatus(TransactionMessage::ACCEPT);

        AcceptOKMessage accept_ok_msg;
        reply.set_op(Reply::ACCEPT_OK);
        reply.set_allocated_accept_ok(&accept_ok_msg);

    }
    transport->SendMessage(this, remote, reply);
}

void Server::HandleCommit(const TransportAddress &remote,
                          const proto::CommitMessage c_msg) {
    uint64_t txn_id = c_msg.txnid();

    vector<uint64_t> deps;
    DependencyList received_dep = c_msg.dep();
    for (int i = 0; i < received_dep.txnid_size(); i++) {
        deps.push_back(received_dep.txnid(i));
    }
    _HandleCommit(txn_id,deps);
}

void Server::_HandleCommit(uint64_t txn_id, std::vector<uint64_t> deps) {
    Transaction txn = id_txn_map[txn_id];
    dep_map[txn_id] = deps;
    txn.setTransactionStatus(TransactionMessage::COMMIT);
    // check if this unblocks others, and rerun HandleCommit for those
    if (blocking_ids.find(txn_id) != blocking_ids.end()) {
        for(uint64_t blocked_id : blocking_ids[txn_id]) {
            Server::_HandleCommit(blocked_id, dep_map[blocked_id]);
        }
    }

    // wait and inquire
    vector<uint64_t> not_committing_ids = _checkIfAllCommitting(id_txn_map, deps);
    if (not_committing_ids.size() != 0) {
        // TODO: if txn_id not involved on S, inquire S

        // HACK: find a more elegant way to do this
        bool found_on_server = false;
        for (uint64_t blocking_txn_id : not_committing_ids) {

            // for every txn_id found on this server, add it to the list of blocking
            if (id_txn_map.find(txn_id) != id_txn_map.end()){
                if (blocking_ids.find(blocking_txn_id) == blocking_ids.end()) {
                    blocking_ids[blocking_txn_id] = vector<uint64_t>(txn_id);
                } else {
                    blocking_ids[blocking_txn_id].push_back(txn_id);
                }
            }
            found_on_server = true;
        }
        // need to wait for local server to set transactions to committing
        if (found_on_server) return;
    }

    // execute phase TODO make this a seperate helper fn

    // init all locally processed status to false
    processed[txn_id] = false;
    for (int dep_id : deps) {
        processed[dep_id] = false;
    }
    _ExecutePhase(txn_id);
}

unordered_map<string, string> Server::Execute(Transaction txn) {
    uint64_t txn_id = txn.getTransactionId();
    unordered_map<string, string> result;

    for (string key : txn.getReadSet()) {
        string val = string();
        store->Get(txn_id, key, val);
        result[key] = val;
    }

    for (pair<string, string> write : txn.getWriteSet()) {
        store->Put(txn_id, write.first, write.second);
        result[write.first] = write.second;
    }

    return result;
}

unordered_map<string, string> Server::_ExecutePhase(uint64_t txn_id) {
    unordered_map<string, string> result;
    vector<uint64_t> deps = dep_map[txn_id];
    while (!processed[txn_id]) {
        // TODO make choosing other_txn_id more efficient
        for (pair<uint64_t, vector<uint64_t>> pair : dep_map) {
            uint64_t other_txn_id = pair.first;
            if (_ReadyToProcess(id_txn_map[other_txn_id])) {
                vector<uint64_t> scc = _StronglyConnectedComponent(txn_id);
                for (int scc_id : scc) {
                    // TODO check if scc_id is involved with S and not abandoned
                    if (scc_id == txn_id) {
                        result = Execute(id_txn_map[scc_id]);
                    } else {
                        Execute(id_txn_map[scc_id]);
                    }
                    processed[scc_id] = true;
                }
            }
        }
    }
    return result;
}

void _DFS(uint64_t txn_id, set<uint64_t> &visited, unordered_map<uint64_t, vector<uint64_t>> dep_map, vector<uint64_t> &v, stack<uint64_t> &s) {
    visited.insert(txn_id);
    v.push_back(txn_id);
    for (vector<uint64_t>::iterator i = dep_map[txn_id].begin(); i != dep_map[txn_id].end(); i++) {
        if(visited.find(*i) != visited.end()) {
            _DFS(*i, visited, dep_map, v, s);
        }
    }
    s.push(txn_id);
}

// return transpose graph of dep_map
unordered_map<uint64_t, vector<uint64_t>> _getTranspose(unordered_map<uint64_t, vector<uint64_t>> dep_map) {
    unordered_map<uint64_t, vector<uint64_t>> transpose;
    for (pair<uint64_t, vector<uint64_t>> pair : dep_map) {
        uint64_t txn_id = pair.first;
        for(vector<uint64_t>::iterator i = dep_map[txn_id].begin(); i != dep_map[txn_id].end(); ++i)
        {
            transpose[*i].push_back(txn_id);
        }
    }
    return transpose;
}

// returns a strongly connected component that contains the given txn_id, if it exists using Kosaraju's
vector<uint64_t> Server::_StronglyConnectedComponent(uint64_t txn_id) {
    set<uint64_t> visited;
    // // create empty stack, do DFS traversal. push vertex to stack at each visit
    stack<uint64_t> s;
    vector<uint64_t> dummy_v; // TODO make this unnecessary
    _DFS(txn_id, visited, dep_map, dummy_v, s);

    // obtain transpose graph
    // TODO also just create the transpose to begin with and build it as txns come in
    unordered_map<uint64_t, vector<uint64_t>> transpose = _getTranspose(dep_map);

    visited.clear();
    // guaranteed to return since stack will contain every txn_id in map
    while (!s.empty()) {
        uint64_t popped_txn_id = s.top();
        s.pop();

        stack<uint64_t> dummy_s; // TODO make this unnecessary
        vector<uint64_t> scc;
        _DFS(popped_txn_id, visited, transpose, scc, dummy_s);
        vector<uint64_t>::iterator it = find(scc.begin(), scc.end(), txn_id);
        if (it != scc.end()) return scc;
    }
}


// checks if txn is ready to be executed
bool Server::_ReadyToProcess(Transaction txn) {
    uint64_t txn_id = txn.getTransactionId();
    if (processed[txn.getTransactionId()] || txn.getTransactionStatus() != TransactionMessage::COMMIT) return false;
    vector<uint64_t> scc = _StronglyConnectedComponent(txn_id);
    vector<uint64_t> deps = dep_map[txn.getTransactionId()];
    for (int other_txn_id : deps) {
        vector<uint64_t>::iterator it = find(scc.begin(), scc.end(), other_txn_id);
        // check if other_txn_id is not in scc and is not ready, return false
        if (it == scc.end() && !processed[other_txn_id]) return false;
    }
    return true;
}

void Server::Load(const string &key, const string &value, const Timestamp timestamp) {
    return;
}

// sort in ascending order
void Server::ResolveContention(vector<uint64_t> scc) {
    sort(scc.begin(), scc.end());
}

// return list of ids that are not committing
vector<uint64_t> _checkIfAllCommitting(
    unordered_map<uint64_t, Transaction> id_txn_map,
    vector<uint64_t> deps
    ) {
        vector<uint64_t> not_committing_ids;
        for (int txn_id : deps) {
            Transaction txn = id_txn_map[txn_id];
            if (txn.getTransactionStatus() != TransactionMessage::COMMIT) {
                not_committing_ids.push_back(txn_id);
            }
        }
        return not_committing_ids;
    }
}
// namespace janusstore
