#include "store/janustore/server.h"
#include "lib/tcptransport.h"

namespace janusstore {

using namespace std;
using namespace proto;

Server::Server()
{
    store = new Store();
}

Server::~Server()
{
    delete store;
}

void
Server::LeaderUpcall(opnum_t opnum, const string &str1, bool &replicate, string &str2)
{
    Panic("Unimplemented!");
}

void
Server::ReplicaUpcall(opnum_t opnum, const string &str1, bool &replicate, string &str2)
{
    Panic("Unimplemented!");
}

void
Server::UnloggedUpcall(const string &str1, string &str2)
{
    Debug("Received Unlogged Request: %s", str1.substr(0,10).c_str());

    Request request;
    Reply reply;

    request.ParseFromString(str1);

    switch (request.op()) {
    case tapirstore::proto::Request::PREACCEPT:
        uint64_t ballot = request.preaccept().ballot();
        TransactionMessage txnMsg = request.preaccept().txn();
        if (ballot < accepted_ballot) {
            reply.set_op(PREACCEPT_NOT_OK);
            break;
        }
        accepted_ballot = ballot;

        // construct the transaction object
        Transaction txn = Transaction(txnMsg.txind());
        txn.setTransactionStatus(PREACCEPT)
        for (int i = 0; i < txn.gets_size(); i++) {
            txn.addReadSet(txn.gets(i).key());
        }
        for (int i = 0; i < txn.puts_size(); i++) {
            PutMessage put = txn.puts(i);
            txn.addWriteSet(put.key(), put.value());
        }

        // add to dependency graph
        id_txn_map[txn.getTranscationId()] = txn;
        if (dep_map.find(txn.getTranscationId()) == dep_map.end()) {
          dep_map[txn.getTranscationId] = [];
        } else {
          // TODO when would this happen?
        }
        // TODO need another map of write/read set to check if txn ids conflict?

        // create dep list
        std::list<uint64_t> dep_list = dep_map[txn.getTranscationId];
        DependencyList dep;
        for (int i = 0; i < dep_list.size(); i++) {
            dep.mutable_txnid(i).set_txnid(dep_list[i]);
        }
        PreAcceptOkMessage preaccept_ok_msg;
        PreAcceptOkMessage.set_txnid(txn.getTranscationId());
        preaccept_ok_msg.set_dep(dep);

        // return accept ok
        // set this txn's status to pre-accepted (with this ballot? TODO)
        reply.set_op(PREACCEPT_OK);
        reply.set_preaccept_ok(preaccept_ok_msg);
        reply.SerializeToString(&str2);
        break;
    case tapirstore::proto::Request::ACCEPT:
        break;
    case tapirstore::proto::Request::COMMIT:
        break;
    case tapirstore::proto::Request::INQUIRE:
        break;
    default:
        Panic("Unrecognized Unlogged request.");
    }
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
//     store->Load(key, value, timestamp);
// }

} // namespace janusstore
