// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
#include "store/janusstore/client.h"

namespace janusstore {

using namespace std;
using namespace proto;

Client::Client(const string configPath, int nShards, 
	int closestReplica, Transport *transport)
	: nshards(nShards), transport(transport) {

		// initialize a random client ID
		client_id = 0;
		while (client_id == 0) {
	        random_device rd;
	        mt19937_64 gen(rd());
	        uniform_int_distribution<uint64_t> dis;
	        client_id = dis(gen);
    	}

    	// MSB = client ID, LSB = txn num
    	txn_id = (client_id/10000)*10000;

		// MSB = client_id, LSB = ballot num
		ballot = (client_id/10000)*10000;

		bclient.reserve(nshards);
		Debug("Initializing Janus client with id [%llu] %lu", client_id, nshards);

		/* Start a client for each shard. */
	    for (uint64_t i = 0; i < nshards; i++) {
	        string shardConfigPath = configPath + to_string(i) + ".config";
	        ShardClient *shardclient = new ShardClient(shardConfigPath,
	                transport, client_id, i, closestReplica);
	        // we use shardclients instead of bufferclients here
	        bclient[i] = shardclient;
	    }

	    Debug("Janus client [%lu] created! %llu %lu", client_id, nshards, bclient.size());
	}
Client::~Client() {
	for (auto b : bclient) {
		delete b;
	}
}

void Client::setParticipants(Transaction *txn) {
	participants.clear();
	for (const auto &key : txn->read_set) {
		int i = key_to_shard(key, nshards);
		if (participants.find(i) == participants.end()) {
    		participants.insert(i);
  		}
	}

	for (const auto &pair : txn->write_set) {
		int i = key_to_shard((pair.first), nshards);
		if (participants.find(i) == participants.end()) {
    		participants.insert(i);
  		}
	}
}

void Client::PreAccept(Transaction *txn, uint64_t ballot, commit_callback ccb) {
	txn->setTransactionId(txn_id);
	txn_id++;
	setParticipants(txn);

	// TODO add the callback to map

	for (auto p : participants) {
		auto pcb = std::bind(&Client::PreAcceptCallback, this,
				txn->getTransactionId(), placeholders::_1, placeholders::_2);
		
		bclient[p]->PreAccept(*txn, ballot, pcb);
	}
}

void Client::Accept(uint64_t txn_id, set<uint64_t> deps, uint64_t ballot) {
	for (auto p : participants) {
		std::vector<uint64_t> vec_deps (deps.begin(), deps.end());
		auto acb = std::bind(&Client::AcceptCallback, this, txn_id, placeholders::_1, placeholders::_2);

		bclient[p]->Accept(txn_id, vec_deps, ballot, acb);
	}
}

void Client::Commit(uint64_t txn_id, set<uint64_t> deps) {
	for (auto p : participants) {
		std::vector<uint64_t> vec_deps (deps.begin(), deps.end());
		auto ccb = std::bind(&Client::CommitCallback, this, txn_id, placeholders::_1, placeholders::_2);

		bclient[p]->Commit(txn_id, vec_deps, ccb);
	}
}

void Client::PreAcceptCallback(uint64_t txn_id, int shard, std::vector<janusstore::proto::Reply> replies) {
	// update dependencies and responded shards
	responded.insert(shard);
	for (auto reply : replies) {
		if (reply.op() == Reply::PREACCEPT_OK) {
			// parse message for deps
			DependencyList msg = reply.preaccept_ok().dep();
			for (int i = 0; i < msg.txnid_size(); i++) {
				uint64_t dep_id = msg.txnid(i);
				// add dep to aggregated set
				// TODO verify this is correct syntax
				aggregated_deps[txn_id].insert(dep_id);
			}
		} else {
			// TODO case where not ok
		}
	}

	// if all shards have responded, move onto Commit stage
	if (responded.size() == participants.size()) {
		// TODO check whether we have a fast quorum before doing commit
		responded.clear();
		Commit(txn_id, aggregated_deps[txn_id]);
	}
}

void Client::AcceptCallback(uint64_t txn_id, int shard, std::vector<janusstore::proto::Reply> replies) {
	responded.insert(shard);
	for (auto reply : replies) {
		if (reply.op() == Reply::ACCEPT_NOT_OK) {
			// TODO case where not ok
		}
	}

	if (responded.size() == participants.size()) {
		// no need to check for a quorum for every shard because we dont implement failure recovery
		responded.clear();
		Commit(txn_id, aggregated_deps[txn_id]);
	}
	return;
}
void Client::CommitCallback(uint64_t txn_id, int shard, std::vector<janusstore::proto::Reply> replies) {
	responded.insert(shard);

	if (responded.size() == participants.size()) {
		// return results to client
		for (auto reply : replies) {
			// TODO how to return results? may also need to update CommitOKMessage
		}
	}
	return;
}
}
