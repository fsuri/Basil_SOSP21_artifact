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
        PreAccept(request, reply);
        break;
    case janusstore::proto::Request::ACCEPT:
        break;
    case janusstore::proto::Request::COMMIT:
        break;
    case janusstore::proto::Request::INQUIRE:
        break;
    default:
        Panic("Unrecognized Unlogged request.");
    }
}

void Server::PreAccept(Request& request, Reply& reply) {
    TransactionMessage txnMsg = request.preaccept().txn();
    uint64_t ballot = request.preaccept().ballot();
    uint64_t txn_id = txnMsg.txnid();
    if (accepted_ballots.find(txn_id) != accepted_ballots.end() &&
     ballot > accepted_ballots[txn_id]) {
        reply.set_op(Reply::PREACCEPT_NOT_OK);
    }
    accepted_ballots[txn_id] = ballot;
    // construct the transaction object
    Transaction txn = Transaction(txn_id);
    txn.setTransactionStatus(PREACCEPT);
    id_txn_map[txn.getTransactionId()] = txn;

    // construct conflicts and read/write sets
    std::vector<uint64_t> dep_list;
    for (int i = 0; i < txnMsg.gets_size(); i++) {
        string key = txnMsg.gets(i).key();
        txn.addReadSet(key);
        if (read_key_txn_map.find(key) == read_key_txn_map.end()) {
          read_key_txn_map[key] = std::vector<uint64_t>(txn_id);
        } else {
          read_key_txn_map[key].push_back(txn_id);
        }

        if (write_key_txn_map.find(key) != write_key_txn_map.end()) {
            std::vector<uint64_t> other_txn_ids = write_key_txn_map[key];
            // append conflicts
            dep_list.insert(dep_list.end(), other_txn_ids.begin(), other_txn_ids.end());
        }
    }

    for (int i = 0; i < txnMsg.puts_size(); i++) {
        PutMessage put = txnMsg.puts(i);
        string key = put.key();
        txn.addWriteSet(put.key(), put.value());

        if (write_key_txn_map.find(key) == write_key_txn_map.end()) {
          write_key_txn_map[key] = std::vector<uint64_t>(txn_id);
        } else {
          // append conflicts
          std::vector<uint64_t> other_txn_ids = write_key_txn_map[key];
          dep_list.insert(dep_list.end(), other_txn_ids.begin(), other_txn_ids.end());
          write_key_txn_map[key].push_back(txn_id);
        }

        if (read_key_txn_map.find(key) != read_key_txn_map.end()) {
          std::vector<uint64_t> other_txn_ids = read_key_txn_map[key];
          // append conflicts
          dep_list.insert(dep_list.end(), other_txn_ids.begin(), other_txn_ids.end());
        }
    }

    // add to dependency graph
    dep_map[txn.getTransactionId()] = dep_list;

    // create dep list
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
    // reply.SerializeToString(&str2);
}

void Server::Accept(Request& request, Reply& reply) {
    AcceptMessage accept_msg = request.accept();
    uint64_t ballot = accept_msg.ballot();
    uint64_t txn_id = accept_msg.txnid();
    uint64_t accepted_ballot = accepted_ballots[txn_id];
    if (id_txn_map[txn_id].getTransactionStatus() == COMMIT || ballot < accepted_ballot) {
        // send back txn id and highest ballot for that txn
        AcceptNotOKMessage accept_not_ok;
        accept_not_ok.set_txnid(txn_id);
        accept_not_ok.set_highest_ballot(accepted_ballot);
        reply.set_op(Reply::PREACCEPT_NOT_OK);
        reply.set_allocated_accept_not_ok(&accept_not_ok);
    } else {
        accepted_ballots[txn_id] = ballot;
        // replace dep_map with the list from the message
        std::vector<uint64_t> dep_list;
        DependencyList received_dep = accept_msg.dep();
        for (int i = 0; i < received_dep.txnid_size(); i++) {
            dep_list.push_back(received_dep.txnid(i));
        }
        dep_map[txn_id] = dep_list;

        id_txn_map[txn_id].setTransactionStatus(ACCEPT);
        reply.set_op(Reply::ACCEPT_OK);
    }

    // TODO serialize the reply
}

void Server::Load(const string &key, const string &value, const Timestamp timestamp) {
    return;
}
// std::map<opid_t, std::string>
// Server::Merge(const std::map<opid_t, std::vector<RecordEntry>> &d,
//               const std::map<opid_t, std::vector<RecordEntry>> &u,
//               const std::map<opid_t, std::string> &majority_results_in_d)
// {
//     Panic("Unimplemented!");
// }

// void
// Server::Load(const string &key, const string &value, const Timestamp timestamp)
// {
//     Panic("Unimplemented!");
// }

} // namespace janusstore
