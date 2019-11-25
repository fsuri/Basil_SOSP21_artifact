// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
#include "store/janusstore/server.h"
#include <stack>

namespace janusstore {

using namespace std;
using namespace proto;

vector<uint64_t> _checkIfAllCommitting(unordered_map<uint64_t, Transaction> id_txn_map, vector<uint64_t> deps);

Server::Server(transport::Configuration &config, int groupIdx, int myIdx, Transport *transport)
    : transport(transport), groupIdx(groupIdx), myIdx(myIdx), config(config)
{
    transport->Register(this, config, groupIdx, myIdx);
    store = new Store();
}

Server::~Server() {
    delete store;
}

void Server::ReceiveMessage(const TransportAddress &remote,
                        const string &type, const string &data,
                        void *meta_data) {
    replication::ir::proto::UnloggedRequestMessage unlogged_request;
    replication::ir::proto::UnloggedReplyMessage unlogged_reply;

    Request request;
    Reply reply;
    Debug("Got type %s", type.c_str());
    if (type == unlogged_request.GetTypeName()) {
        unlogged_request.ParseFromString(data);
        uint64_t clientreqid = unlogged_request.req().clientreqid();
        unlogged_reply.set_clientreqid(clientreqid);

        request.ParseFromString(unlogged_request.req().op());
        Debug("Got, %s", request.DebugString().c_str());
        switch(request.op()) {
            case Request::PREACCEPT: {
                HandlePreAccept(remote, request.preaccept(), &unlogged_reply);
                break;
            }
            case Request::ACCEPT: {
                Debug("[Server %i] Received ACCEPT message", this->myIdx);
                HandleAccept(remote, request.accept(), &unlogged_reply);
                break;
            }
            case Request::COMMIT: {
                Debug("[Server %i] Received COMMIT message", this->myIdx);
                HandleCommit(remote, request.commit(), &unlogged_reply);
                break;
            }
            case Request::GET: {
                string key = request.key();
                string value = store->Read(key);

                reply.set_key(key);
                reply.set_value(value);

                unlogged_reply.set_reply(reply.SerializeAsString());
                transport->SendMessage(this, remote, unlogged_reply);
                break;
            }
            default: {
                Panic("Unrecognized request.");
                break;
            }
        }
    } else if (type == reply.GetTypeName()) {
        reply.ParseFromString(data);
        if (reply.op() == Reply::INQUIRE_OK) {
            HandleInquireReply(reply.inquire_ok());
        } else {
            Panic("Unrecognized reply message in server");
        }
    } else if (type == request.GetTypeName()) {
        // handle inquiries
        request.ParseFromString(data);
        if (request.op() == Request::INQUIRE) {
            HandleInquire(remote, request.inquire(), &reply);
        } else {
            Panic("Should only directly handle inquiries!");
        }
    } else {
        Panic("Unrecognized message.");
    }
}

void
Server::HandlePreAccept(const TransportAddress &remote,
                        const PreAcceptMessage &pa_msg,
                        replication::ir::proto::UnloggedReplyMessage *unlogged_reply)
{
    Reply reply;
    TransactionMessage txnMsg = pa_msg.txn();
    uint64_t txn_id = txnMsg.txnid();
    uint64_t ballot = pa_msg.ballot();

    Debug("[Server %i] Received PREACCEPT message for txn %s",
        this->myIdx, txnMsg.DebugString().c_str());

    // construct the transaction object
    // TODO might be able to optimize this process somehow
    Transaction txn = Transaction(txn_id, txnMsg);
    vector<uint64_t>* dep_list_ptr = BuildDepList(txn, ballot);

    if (dep_list_ptr == NULL) {
        // return not ok
        PreAcceptNotOKMessage pa_not_ok_msg;
        pa_not_ok_msg.set_txnid(txn_id);

        reply.set_op(Reply::PREACCEPT_NOT_OK);
        reply.set_allocated_preaccept_not_ok(&pa_not_ok_msg);
        unlogged_reply->set_reply(reply.SerializeAsString());
        Debug("[Server %i] sending PREACCEPT NOT-OK message for txn %llu",
        this->myIdx, txn_id);
        transport->SendMessage(this, remote, *unlogged_reply);
        reply.release_preaccept_not_ok();
        return;
    }

    vector<uint64_t> dep_list = *dep_list_ptr;

    // create dep list for reply
    DependencyList dep;
    PreAcceptOKMessage preaccept_ok_msg;
    preaccept_ok_msg.set_txnid(txn_id);

    for (int i = 0; i < dep_list.size(); i++) {
        dep.add_txnid(dep_list[i]);
        DependencyMeta* depmeta = preaccept_ok_msg.add_depmeta();

        uint64_t dep_id = dep_list[i];
        depmeta->set_txnid(dep_id);

        // if we know what shards dep_id participates in
        Transaction dep_txn = NULL;
        if (id_txn_map.find(dep_id) != id_txn_map.end()) {
            dep_txn = id_txn_map[dep_id];
            for (int group : dep_txn.groups) {
                depmeta->add_group(group);
            }
        }
    }

    preaccept_ok_msg.set_allocated_dep(&dep);

    reply.set_op(Reply::PREACCEPT_OK);
    reply.set_allocated_preaccept_ok(&preaccept_ok_msg);

    unlogged_reply->set_reply(reply.SerializeAsString());

    Debug("[Server %i] sending PREACCEPT-OK message for txn %llu %s",
        this->myIdx, txn_id,
        reply.DebugString().c_str());
    transport->SendMessage(this, remote, *unlogged_reply);

    preaccept_ok_msg.release_dep();
    reply.release_preaccept_ok();
}

vector<uint64_t>* Server::BuildDepList(Transaction &txn, uint64_t ballot) {
    uint64_t txn_id = txn.getTransactionId();
    if (accepted_ballots.find(txn_id) != accepted_ballots.end() &&
     ballot > accepted_ballots[txn_id]) {
        // TODO less hacky way
        Debug("[Server %i] Sending PREACCEPT NOT-OK since %llu > %llu",
            this->myIdx, ballot, accepted_ballots[txn_id]);
        return NULL;
    }
    accepted_ballots[txn_id] = ballot;

    txn.setTransactionStatus(TransactionMessage::PREACCEPT);
    id_txn_map[txn.getTransactionId()] = txn;

    // construct conflicts and read/write sets
    set<uint64_t> dep_set;
    for (auto key : txn.getReadSet()) {
        if (read_key_txn_map.find(key) == read_key_txn_map.end()) {
          read_key_txn_map[key] = set<uint64_t>{txn_id};
        } else {
          read_key_txn_map[key].insert(txn_id);
        }

        // append conflicts
        if (write_key_txn_map.find(key) != write_key_txn_map.end()) {
            // Debug("here and idk why");
            set<uint64_t> other_txn_ids = write_key_txn_map[key];
            dep_set.insert(other_txn_ids.begin(), other_txn_ids.end());
        }
    }


    for (auto const& kv : txn.getWriteSet()) {
        string key = kv.first;
        if (write_key_txn_map.find(key) == write_key_txn_map.end()) {
          write_key_txn_map[key] = set<uint64_t>{txn_id};
        } else {
          // append conflicts
          set<uint64_t> other_txn_ids = write_key_txn_map[key];
          dep_set.insert(other_txn_ids.begin(), other_txn_ids.end());
          write_key_txn_map[key].insert(txn_id);
        }

        if (read_key_txn_map.find(key) != read_key_txn_map.end()) {
          // Debug("should be here and that might be a problem");
          // append conflicts
          set<uint64_t> other_txn_ids = read_key_txn_map[key];
          // TODO remove, but this is useful for debuggign
          // Debug("other txn ids read in write confl %i", other_txn_ids.size());
          // for (auto i = other_txn_ids.begin(); i != other_txn_ids.end(); ++i)
          //   cout << *i << ' ';
          if (other_txn_ids.find(txn_id) != other_txn_ids.end()) {
            // do nothing
          } else {
            dep_set.insert(other_txn_ids.begin(), other_txn_ids.end());
          }
        }
    }

    vector<uint64_t> dep_vec(dep_set.begin(), dep_set.end());

    // add to dependency graph
    dep_map[txn_id] = dep_vec;
    vector<uint64_t>* dep_vec_ptr = new std::vector<uint64_t>(dep_vec);
    return dep_vec_ptr;
}

void Server::HandleAccept(const TransportAddress &remote,
                          const proto::AcceptMessage &a_msg,
                          replication::ir::proto::UnloggedReplyMessage *unlogged_reply)
{
    Reply reply;
    uint64_t ballot = a_msg.ballot();
    uint64_t txn_id = a_msg.txnid();
    Transaction *txn = &id_txn_map[txn_id];

    // reconstruct dep_list from message
    vector<uint64_t> msg_deps;
    DependencyList received_dep = a_msg.dep();
    for (int i = 0; i < received_dep.txnid_size(); i++) {
        msg_deps.push_back(received_dep.txnid(i));
    }
    uint64_t accepted_ballot = accepted_ballots[txn_id];

    if (txn->getTransactionStatus() == TransactionMessage::COMMIT || ballot < accepted_ballot) {
        // send back txn id and highest ballot for that txn
        AcceptNotOKMessage accept_not_ok_msg;
        accept_not_ok_msg.set_txnid(txn_id);
        accept_not_ok_msg.set_highest_ballot(accepted_ballot);

        reply.set_op(Reply::ACCEPT_NOT_OK);
        reply.set_allocated_accept_not_ok(&accept_not_ok_msg);

        Debug("[Server %i] sending ACCEPT-NOT-OK message for txn %llu %s",
            this->myIdx, txn_id,
            reply.DebugString().c_str());

        unlogged_reply->set_reply(reply.SerializeAsString());
        transport->SendMessage(this, remote, *unlogged_reply);

        reply.release_accept_not_ok();
    } else {
        // replace dep_map with the list from the message
        dep_map[txn_id] = msg_deps;

        // update highest ballot with passed in ballot
        accepted_ballots[txn_id] = ballot;

        // update txn status to accept
        txn->setTransactionStatus(TransactionMessage::ACCEPT);

        AcceptOKMessage accept_ok_msg;
        accept_ok_msg.set_txnid(txn_id);

        reply.set_op(Reply::ACCEPT_OK);
        reply.set_allocated_accept_ok(&accept_ok_msg);

        Debug("[Server %i] sending ACCEPT-OK message for txn %llu %s",
            this->myIdx, txn_id,
            reply.DebugString().c_str());

        unlogged_reply->set_reply(reply.SerializeAsString());
        transport->SendMessage(this, remote, *unlogged_reply);

        reply.release_accept_ok();
    }
}

void Server::HandleCommit(const TransportAddress &remote,
                          const proto::CommitMessage c_msg,
                          replication::ir::proto::UnloggedReplyMessage *unlogged_reply) {
    uint64_t txn_id = c_msg.txnid();

    vector<uint64_t> deps;
    DependencyList received_dep = c_msg.dep();
    for (int i = 0; i < received_dep.txnid_size(); i++) {
        deps.push_back(received_dep.txnid(i));
    }
    dep_map[txn_id] = deps;

    // for each dep, get its participant shards
    unordered_map<uint64_t, vector<int>> dep_shards;
    for (int i = 0; i < c_msg.depmeta_size(); i++) {
        DependencyMeta depmeta = c_msg.depmeta(i);
        vector<int> participant_shards;
        for (int j = 0; j < depmeta.group_size(); j++) {
            participant_shards.push_back(depmeta.group(j));
        }
        dep_shards[depmeta.txnid()] = participant_shards;
    }

    depshards_map[txn_id] = dep_shards;

    // Debug("gonna handle commit for %llu, message: %s", txn_id, c_msg.DebugString().c_str());
    _HandleCommit(txn_id, remote, unlogged_reply);
}

void Server::_HandleCommit(uint64_t txn_id,
                           const TransportAddress &remote,
                           replication::ir::proto::UnloggedReplyMessage *unlogged_reply) {
    Transaction *txn = &id_txn_map[txn_id];
    vector<uint64_t> deps = dep_map[txn_id];

    txn->setTransactionStatus(TransactionMessage::COMMIT);
    Debug("Set txn id %llu to COMMIT", txn_id);
    // check if this unblocks others, and rerun HandleCommit for those
    if (blocking_ids.find(txn_id) != blocking_ids.end()) {
        for(auto blocked_id : blocking_ids[txn_id]) {
            Debug("Found blocked id %llu for txn id %llu", blocked_id, txn_id);
            Transaction *txn = &id_txn_map[blocked_id];
            txn->blocked_by_list.erase(txn_id);
            if (txn->blocked_by_list.empty()) {
                uint64_t request_id = txn->request_id;
                Debug("Blocked id %llu commitable now", blocked_id);
                for (auto *client_addr : txn->client_addrs){
                    replication::ir::proto::UnloggedReplyMessage blocked_unlogged_reply;
                    blocked_unlogged_reply.set_clientreqid(request_id);
                    _HandleCommit(blocked_id, *client_addr, &blocked_unlogged_reply);
                    blocked_unlogged_reply.Clear();
                }
                txn->client_addrs.clear();
            }
        }
        blocking_ids.erase(txn_id);
    }

    // once txn becomes committing, see if you have to send inquire to any servers
    if (inquired_ids.find(txn_id) != inquired_ids.end()) {
        for(auto pair : inquired_ids[txn_id]) {
            Reply reply;
            HandleInquire(*pair.first, pair.second, &reply);
        }
        inquired_ids.erase(txn_id);
    }

    // wait and inquire
    vector<uint64_t> not_committing_ids = _checkIfAllCommitting(id_txn_map, deps);
    if (not_committing_ids.size() != 0) {
        // Debug("Need to block %llu", txn_id);
        txn->client_addrs.insert(&remote);

        // HACK: find a more elegant way to do this
        bool found_on_server = false;
        for (uint64_t blocking_txn_id : not_committing_ids) {
            Debug("%llu blocked by %llu", txn_id, blocking_txn_id);
            txn->blocked_by_list.insert(blocking_txn_id);
            txn->request_id = unlogged_reply->clientreqid();

            if (id_txn_map.find(blocking_txn_id) != id_txn_map.end()){
                found_on_server = true;
            } else {
                // inquire about the status of this transaction
                _SendInquiry(txn_id, blocking_txn_id);
            }

            if (blocking_ids.find(blocking_txn_id) == blocking_ids.end()) {
                blocking_ids[blocking_txn_id] = set<uint64_t>{txn_id};
            } else {
                blocking_ids[blocking_txn_id].insert(txn_id);
            }
        }
        // need to wait for local server to set transactions to committing
        if (found_on_server) return;
    }

    // initialize unknown ids to false
    for (int dep_id : deps) {
        if (processed.find(dep_id) == processed.end()) {
            processed[dep_id] = false;
        }
    }

    _ExecutePhase(txn_id, remote, unlogged_reply);
}

void Server::HandleInquire(const TransportAddress &remote,
                           const proto::InquireMessage i_msg,
                           Reply *reply) {

    uint64_t txn_id = i_msg.txnid();

    Debug("[Server %i] on shard %i received inquiry for transaction %llu", myIdx, groupIdx, txn_id);
    Transaction txn = this->id_txn_map[txn_id];

    // after txn_id is in the committing stage, return the deps list
    if (txn.getTransactionStatus() == TransactionMessage::COMMIT) {
        vector<uint64_t> deps = this->dep_map[txn_id];
        InquireOKMessage payload;
        DependencyList dep_list;

        reply->set_op(Reply::INQUIRE_OK);

        Debug("%i", reply->op());
        reply->mutable_inquire_ok()->set_txnid(txn_id);

        for (auto id : deps) {
            reply->mutable_inquire_ok()->mutable_dep()->add_txnid(id);
        }

        transport->SendMessage(this, remote, *reply);
    } else {
        if (inquired_ids.find(txn_id) != inquired_ids.end()) {
            inquired_ids[txn_id].push_back(make_pair(&remote, i_msg));
        } else {
            vector<pair<const TransportAddress*, proto::InquireMessage>> pairs{make_pair(&remote, i_msg)};
            inquired_ids[txn_id] = pairs;
        }
    }
}

void Server::HandleInquireReply(const proto::InquireOKMessage i_ok_msg) {
    uint64_t txn_id = i_ok_msg.txnid();
    Transaction *txn = &id_txn_map[txn_id];

    if (txn->getTransactionStatus() != TransactionMessage::COMMIT) {
        vector<uint64_t> msg_deps;
        DependencyList received_dep = i_ok_msg.dep();
        for (int i = 0; i < received_dep.txnid_size(); i++) {
            msg_deps.push_back(received_dep.txnid(i));
        }
        dep_map[txn_id] = msg_deps;

        // set this txn id to committing because we received this reply
        txn->setTransactionStatus(TransactionMessage::COMMIT);
        for (auto blocked_id : blocking_ids[txn_id]) {
            Transaction *blocked_txn = &id_txn_map[blocked_id];
            Debug("Blocked id %llu commitable now due to inquiry", blocked_id);
            uint64_t request_id = txn->request_id;
            for (auto *client_addr : blocked_txn->client_addrs){
                replication::ir::proto::UnloggedReplyMessage blocked_unlogged_reply;
                blocked_unlogged_reply.set_clientreqid(request_id);
                _HandleCommit(blocked_id, *client_addr, &blocked_unlogged_reply);
                blocked_unlogged_reply.Clear();
            }
            blocked_txn->client_addrs.clear();
        }
        blocking_ids.erase(txn_id);
    }
}

void Server::_SendInquiry(uint64_t txn_id, uint64_t blocking_txn_id) {
    Debug("[Server %i] on shard %i sending inquiry for transaction %llu", myIdx, groupIdx, blocking_txn_id);

    unordered_map<uint64_t, vector<int>> dep_shards = this->depshards_map[txn_id];
    vector<int> relevant_groups = dep_shards[blocking_txn_id];
    // Debug("Found %llu groups processing the blocking txn %llu", relevant_groups.size(), blocking_txn_id);

    // TODO dont overload one group/replica
    uint64_t nearest_replica = 0;
    int nearest_group;
    for (auto group : relevant_groups) {
        if (group != this->groupIdx) {
            nearest_group = group;
            break;
        }
    }
    Request request;
    request.set_op(Request::INQUIRE);
    request.mutable_inquire()->set_txnid(blocking_txn_id);

    Debug("Inquiring replica %llu at group %d for blocking txn %llu", nearest_replica, nearest_group, blocking_txn_id);

    transport->SendMessageToReplica(
        this, nearest_group, nearest_replica, request);
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

void Server::_ExecutePhase(uint64_t txn_id,
                           const TransportAddress &remote,
                           replication::ir::proto::UnloggedReplyMessage *unlogged_reply
) {
    Debug("In execute phase for %llu", txn_id);
    unordered_map<string, string> result;
    vector<uint64_t> deps = dep_map[txn_id];
    Transaction *txn = &id_txn_map[txn_id];
    uint64_t num_deps = deps.size();
    // Debug("%s\n", ("num_deps is " + to_string(num_deps)).c_str());
    uint64_t num_deps_processed = 0;

    while (!processed[txn_id]) {
        for (pair<uint64_t, vector<uint64_t>> pair : dep_map) {
            uint64_t other_txn_id = pair.first;
            // Debug("Checking if %llu is ready to process!", other_txn_id);
            if (num_deps_processed == num_deps) {
                processed[txn_id] = true;
                break;
            }
            if (processed[other_txn_id]) {
                // Debug("In here");
                num_deps_processed++;
                continue;
            }
            if (_ReadyToProcess(id_txn_map[other_txn_id])) {
                Debug("%llu is ready to process!", other_txn_id);
                vector<uint64_t> scc = _StronglyConnectedComponent(other_txn_id);
                ResolveContention(scc);
                // TODO: txn_id is not present in scc when it should be
                // Debug("%llu has scc of size %llu!", other_txn_id, scc.size());
                for (uint64_t scc_id : scc) {
                    Transaction *scc_txn = &id_txn_map[scc_id];
                    scc_txn->setResult(Execute(id_txn_map[scc_id]));
                    Debug("[Server %i] executing transaction %llu", myIdx, scc_id);
                    processed[scc_id] = true;
                }
                num_deps_processed++;
            }
        }
    }

    txn->setResult(Execute(id_txn_map[txn_id]));

    stats.Increment("commits", 1);

    Debug("[Server %i] executing transaction %llu", myIdx, txn_id);
    processed[txn_id] = true;

    result = txn->result;

    Debug("Sending CommitOK");
    CommitOKMessage commit_ok;
    Reply reply;

    // construct proto
    for (auto res : result) {
        // Debug("Adding %s, %s", res.first.c_str(), res.second.c_str());
        PutMessage *pair = commit_ok.add_pairs();
        pair->set_key(res.first);
        pair->set_value(res.second);
    }

    commit_ok.set_txnid(txn_id);

    reply.set_allocated_commit_ok(&commit_ok);
    reply.set_op(Reply::COMMIT_OK);

    unlogged_reply->set_reply(reply.SerializeAsString());

    // Debug("[Server %i] COMMIT, sending back %s", this->myIdx, reply.DebugString().c_str());

    transport->SendMessage(this, remote, *unlogged_reply);

    reply.release_commit_ok();
}

void _DFS(uint64_t txn_id,
    unordered_map<uint64_t, bool> &visited,
    unordered_map<uint64_t, vector<uint64_t>> dep_map,
    vector<uint64_t> &v,
    std::stack<uint64_t> &s)
{
    visited[txn_id] = true;
    v.push_back(txn_id);
    for (auto i : dep_map[txn_id]) {
        if(!visited[i]) {
            _DFS(i, visited, dep_map, v, s);
        }
    }
    s.push(txn_id);
}

// return transpose graph of dep_map
unordered_map<uint64_t, vector<uint64_t>> _getTranspose(unordered_map<uint64_t, vector<uint64_t>> dep_map) {
    unordered_map<uint64_t, vector<uint64_t>> transpose;
    for (pair<uint64_t, vector<uint64_t>> pair : dep_map) {
        uint64_t txn_id = pair.first;
        for(auto i = dep_map[txn_id].begin(); i != dep_map[txn_id].end(); ++i)
        {
            transpose[*i].push_back(txn_id);
        }
    }
    return transpose;
}

// returns a strongly connected component that contains the given txn_id, if it exists using Kosaraju's
vector<uint64_t> Server::_StronglyConnectedComponent(uint64_t txn_id) {

    unordered_map<uint64_t, bool> visited;
    // create empty stack, do DFS traversal. push vertex to stack at each visit
    std::stack<uint64_t> s;
    vector<uint64_t> dummy_v;
    for (auto kv : dep_map) {
        if(visited.find(kv.first) == visited.end()) {
            visited[kv.first] = false;
            _DFS(kv.first, visited, dep_map, dummy_v, s);
        }
    }

    // obtain transpose graph
    // TODO also just create the transpose to begin with and build it as txns come in
    unordered_map<uint64_t, vector<uint64_t>> transpose = _getTranspose(dep_map);

    for(auto kv : visited) {
        visited[kv.first] = false;
    }
    // guaranteed to return since stack will contain every txn_id in map
    while (!s.empty()) {
        uint64_t popped_txn_id = s.top();
        s.pop();
        if(!visited[popped_txn_id]) {
            stack<uint64_t> dummy_s;
            vector<uint64_t> scc;
            _DFS(popped_txn_id, visited, transpose, scc, dummy_s);
            vector<uint64_t>::iterator it = find(scc.begin(), scc.end(), txn_id);
            if (it != scc.end()) return scc;
        }
    }
    Panic("Should have returned!");
}


// checks if txn is ready to be executed
bool Server::_ReadyToProcess(Transaction txn) {
    uint64_t txn_id = txn.getTransactionId();
    if (processed[txn.getTransactionId()] || txn.getTransactionStatus() != TransactionMessage::COMMIT) return false;
    vector<uint64_t> scc = _StronglyConnectedComponent(txn_id);

    vector<uint64_t> deps = dep_map[txn_id];
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
void Server::ResolveContention(vector<uint64_t> &scc) {
    sort(scc.begin(), scc.end());
}

// return list of ids that are not committing
vector<uint64_t> _checkIfAllCommitting(
    unordered_map<uint64_t, Transaction> id_txn_map,
    vector<uint64_t> deps
    ) {
        vector<uint64_t> not_committing_ids;
        for (uint64_t txn_id : deps) {
            // Debug("Checking if %llu is committing", txn_id);
            Transaction txn = id_txn_map[txn_id];
            if (txn.getTransactionStatus() != TransactionMessage::COMMIT) {
                not_committing_ids.push_back(txn_id);
            }
        }
        Debug("Returning %lu uncommitted ids", not_committing_ids.size());
        return not_committing_ids;
    }
}
// namespace janusstore
