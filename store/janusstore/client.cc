// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
#include "store/janusstore/client.h"

namespace janusstore {
using namespace std;

Client::Client(const std::string configPath, int nShards, 
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

    	// txn_id = bit concat of client_id and txn_num
		txn_num = 0;

		// ballot = bit concat of client_id and ballot_num
		ballot_num = 0;

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
	// TODO where would we set [txn.t_id]?
	// Client assumes Transaction is already constructed	
	setParticipants(txn);
	TransactionMessage *txn_message;
	txn->serialize(txn_message);

	for (auto p : participants) {
		// TODO make ballot_num an actual ballot
		// TODO how will the shardclients notify this client?
		bclient[p]->PreAccept(client_id, *txn, ballot_num, std::bind(
				&ShardClient::PreAcceptCallback, placeholders::_1, placeholders::_2, placeholders::_3)
		);
	}
}
}