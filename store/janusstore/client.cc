// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
#include "store/janusstore/client.h"

namespace janusstore {
using namespace std;

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
		Debug("Initializing Janus client with id [%lu] %lu", client_id, nshards);

		/* Start a client for each shard. */
	    for (uint64_t i = 0; i < nshards; i++) {
	        string shardConfigPath = configPath + to_string(i) + ".config";
	        ShardClient *shardclient = new ShardClient(shardConfigPath,
	                transport, client_id, i, closestReplica);
	        // we use shardclients instead of bufferclients here
	        bclient[i] = shardclient;
	    }

	    Debug("Janus client [%lu] created! %lu %lu", client_id, nshards, bclient.size());
	}
Client::~Client() {
	for (auto b : bclient) {
		delete b;
	}
}

void Client::setParticipants(Transaction *txn) {
	for (const auto &key : txn->readSet) {
		int i = key_to_shard(key, nshards);
		if (participants.find(i) == participants.end()) {
    		participants.insert(i);
  		}
	}

	for (const auto &pair : txn->writeSet) {
		int i = key_to_shard(&(pair->first), nshards);
		if (participants.find(i) == participants.end()) {
    		participants.insert(i);
  		}
	}
}

void Client::PreAccept(Transaction *txn, uint64_t ballot) {
	txn->setTransactionId(txn_id);
	txn_id++;

	setParticipants(txn);
	TransactionMessage *txn_message;
	txn->serialize(txn_message);

	for (auto p : participants) {
		// TODO how will the shardclients notify this client?
		bclient[p]->PreAccept(*txn, ballot,
			std::bind(&Client::PreAcceptCallback, 
				placeholders::_1, placeholders::_2, placeholders::_3)
		);
	}
}

void Client::Accept(uint64_t txn_id, vector<uint64_t> deps, uint64_t ballot) {
	for (auto p : participants) {
		bclient[p]->Accept(txn_id, deps, ballot,
			std::bind(&Client::AcceptCallback, placeholders::_1)
		);
	}
}

void Client::Commit(uint64_t txn_id, vector<uint64_t> deps) {
	for (auto p : participants) {
		bclient[p]->Commit(txn_id, deps, 
			std::bind(&Client::CommitCallback,
				placeholders::_1, placeholders::_2)
		);
	}
}

void Client::PreAcceptCallback(uint64_t txn_id, int status, std::unordered_map<uint64_t, std::vector<uint64_t>> deps) {
	// TODO implement
	return;
}

void Client::AcceptCallback(uint64_t txn_id) {
	// TODO implement
	return;
}
void Client::CommitCallback(uint64_t txn_id, std::vector<uint64_t> results) {
	// TODO implement
	return;
}
}
