// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
#include "store/janusstore/server.h"
#include "lib/tcptransport.h"

namespace janusstore {

using namespace std;
using namespace proto;

Server::Server() {
    store = new Store();
}

Server::~Server() {
    delete store;
}

void Server::UnloggedUpcall(const string &str1, string &str2) {
    Debug("Received Unlogged Request: %s", str1.substr(0,10).c_str());

    Request request;
    Reply reply;

    request.ParseFromString(str1);

    switch (request.op()) {
    case janusstore::proto::Request::PREACCEPT:
    {
        TransactionMessage txnMsg = request.preaccept().txn();
        uint64_t txn_id = txnMsg.txnid();
        uint64_t ballot = request.preaccept().ballot();

        // construct the transaction object
        Transaction txn = Transaction(txn_id);
        for (int i = 0; i < txnMsg.gets_size(); i++) {
            string key = txnMsg.gets(i).key();
            txn.addReadSet(key);
        }

        for (int i = 0; i < txnMsg.puts_size(); i++) {
            PutMessage put = txnMsg.puts(i);
            txn.addWriteSet(put.key(), put.value());
        }

        std::vector<uint64_t> dep_list = HandlePreAccept(txn, ballot);

        // TODO less hacky way
        if (!dep_list.empty() && dep_list.at(0) == -1) {
            reply.set_op(Reply::PREACCEPT_NOT_OK);
            break;
        }

        // create dep list for reply
        DependencyList dep;
        for (int i = 0; i < dep_list.size(); i++) {
            dep.add_txnid(dep_list[i]);
        }
        PreAcceptOKMessage preaccept_ok_msg;
        preaccept_ok_msg.set_txnid(txn.getTransactionId());
        preaccept_ok_msg.set_allocated_dep(&dep);

        // return accept ok
        // set this txn's status to pre-accepted (with this ballot? TODO)
        reply.set_op(Reply::PREACCEPT_OK);
        reply.set_allocated_preaccept_ok(&preaccept_ok_msg);

        // TODO fix this, it's not correct
        reply.SerializeToString(&str2);
        break;
    }
    case janusstore::proto::Request::ACCEPT:
    {
        AcceptMessage accept_msg = request.accept();
        uint64_t ballot = accept_msg.ballot();
        uint64_t txn_id = accept_msg.txnid();

        // reconstruct dep_list from message
        std::vector<uint64_t> msg_dep_list;
        DependencyList received_dep = accept_msg.dep();
        for (int i = 0; i < received_dep.txnid_size(); i++) {
            msg_dep_list.push_back(received_dep.txnid(i));
        }
        uint64_t highest_ballot = HandleAccept(id_txn_map[txn_id], msg_dep_list, ballot);

        if (highest_ballot != -1) {
            // send back txn id and highest ballot for that txn
            AcceptNotOKMessage accept_not_ok;
            accept_not_ok.set_txnid(txn_id);
            accept_not_ok.set_highest_ballot(highest_ballot);
            reply.set_op(Reply::PREACCEPT_NOT_OK);
            reply.set_allocated_accept_not_ok(&accept_not_ok);
            break;
        }
        reply.set_op(Reply::ACCEPT_OK);
        reply.SerializeToString(&str2);
        break;
    }
    case janusstore::proto::Request::COMMIT:
        break;
    case janusstore::proto::Request::INQUIRE:
        break;
    default:
        Panic("Unrecognized Unlogged request.");
    }
}

std::vector<uint64_t> Server::HandlePreAccept(Transaction txn, uint64_t ballot) {
    uint64_t txn_id = txn.getTransactionId();
    if (accepted_ballots.find(txn_id) != accepted_ballots.end() &&
     ballot > accepted_ballots[txn_id]) {
        // TODO less hacky way
        return std::vector<uint64_t>(-1);
    }
    accepted_ballots[txn_id] = ballot;

    txn.setTransactionStatus(PREACCEPT);
    id_txn_map[txn.getTransactionId()] = txn;

    // construct conflicts and read/write sets
    std::vector<uint64_t> dep_list;
    for (auto key : txn.getReadSet()) {
        if (read_key_txn_map.find(key) == read_key_txn_map.end()) {
          read_key_txn_map[key] = std::vector<uint64_t>(txn_id);
        } else {
          read_key_txn_map[key].push_back(txn_id);
        }

        // append conflicts
        if (write_key_txn_map.find(key) != write_key_txn_map.end()) {
            std::vector<uint64_t> other_txn_ids = write_key_txn_map[key];
            dep_list.insert(dep_list.end(), other_txn_ids.begin(), other_txn_ids.end());
        }
    }

    for (auto const& kv : txn.getWriteSet()) {
        string key = kv.first;
        if (write_key_txn_map.find(key) == write_key_txn_map.end()) {
          write_key_txn_map[key] = std::vector<uint64_t>(txn_id);
        } else {
          // append conflicts
          std::vector<uint64_t> other_txn_ids = write_key_txn_map[key];
          dep_list.insert(dep_list.end(), other_txn_ids.begin(), other_txn_ids.end());
          write_key_txn_map[key].push_back(txn_id);
        }

        if (read_key_txn_map.find(key) != read_key_txn_map.end()) {
          // append conflicts
          std::vector<uint64_t> other_txn_ids = read_key_txn_map[key];
          dep_list.insert(dep_list.end(), other_txn_ids.begin(), other_txn_ids.end());
        }
    }

    // add to dependency graph
    dep_map[txn.getTransactionId()] = dep_list;
    return dep_list;
}

uint64_t Server::HandleAccept(Transaction &txn, std::vector<std::uint64_t> msg_deps, uint64_t ballot) {
    uint64_t txn_id = txn.getTransactionId();
    uint64_t accepted_ballot = accepted_ballots[txn_id];
    if (id_txn_map[txn_id].getTransactionStatus() == COMMIT || ballot < accepted_ballot) {
        return accepted_ballot;
    }

    // replace dep_map with the list from the message
    dep_map[txn_id] = msg_deps;

    // update highest ballot with passed in ballot
    accepted_ballots[txn_id] = ballot;

    // update txn status to accept
    txn.setTransactionStatus(ACCEPT);
    return -1;
}

// returns a strongly connected component that contains the given txn_id, if it exists
std::vector<uint64_t> _StronglyConnectedComponent(std::unordered_map<uint64_t, std::vector<uint64_t>> dep_map, uint64_t txn_id) {

}

bool _checkIfAllCommitting(std::unordered_map<uint64_t, Transaction> id_txn_map, std::vector<uint64_t> deps) {
    for (int txn_id : deps) {
        Transaction txn = id_txn_map[txn_id];
        if (txn.getTransactionStatus() != COMMIT) return false;
    }
    return true;
}

void Server::HandleCommit(uint64_t txn_id, std::vector<uint64_t> deps) {
    Transaction txn = id_txn_map[txn_id];
    dep_map[txn_id] = deps;
    txn.setTransactionStatus(COMMIT);

    // wait and inquire
    while(!_checkIfAllCommitting(id_txn_map, deps)) {
        // if txn_id not involved on S
        // inquire S

        // wait for T to be committing
    }

    // execute phase TODO make this a seperate helper fn

    // init all locally processed status to false
    processed[txn_id] = false;
    for (int dep_id : deps) {
        processed[dep_id] = false;
    }

    while (!processed[txn_id]) {

    }

}

// checks if txn is ready to be executed
bool _ReadyToProcess(Transaction txn,
    std::unordered_map<uint64_t, std::vector<uint64_t>> dep_map,
    std::unordered_map<uint64_t, bool> processed) {
    uint64_t txn_id = txn.getTransactionId();
    if (processed[txn.getTransactionId()] || txn.getTransactionStatus() != COMMIT) return false;
    std::vector<uint64_t> scc = _StronglyConnectedComponent(dep_map, txn_id);
    std::vector<uint64_t> deps = dep_map[txn.getTransactionId()];
    for (int other_txn_id : deps) {
        std::vector<uint64_t>::iterator it = std::find(scc.begin(), scc.end(), other_txn_id);
        // check if other_txn_id is not in scc and is not ready, return false
        if (it == scc.end() && !processed[other_txn_id]) return false;
    }
    return true;
}

void Server::Load(const string &key, const string &value, const Timestamp timestamp) {
    return;
}

std::vector<uint64_t> Server::ResolveContention(std::vector<uint64_t> scc) {
    // just return a sorted order lol
}

} // namespace janusstore
