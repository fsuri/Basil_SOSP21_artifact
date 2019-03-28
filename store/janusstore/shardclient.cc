// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
#include "store/tapirstore/shardclient.h"

namespace janusstore {

using namespace std;
using namespace proto;

ShardClient::ShardClient(const string &configPath, Transport *transport,
	uint64_t client_id, int shard, int closestReplica)
	: client_id(client_id), transport(transport), shard(shard) {
  
  ifstream configStream(configPath);
  if (configStream.fail()) {
    Panic("Unable to read configuration file: %s\n", configPath.c_str());
  }

  transport::Configuration config(configStream);
  this->config = &config;

  client = new replication::ir::IRClient(config, transport, client_id);

  if (closestReplica == -1) {
    replica = client_id % config.n;
  } else {
    replica = closestReplica;
  }
  Debug("Sending unlogged to replica %i", replica);
}

ShardClient::~ShardClient() {
    delete client;
}

/* TODO:
1. do we need to pass in [this] as well to the ShardClient's callbck
2. maintain state about each transaction's responses for each phase
3. Implement the callbacks
*/

void ShardClient::PreAccept(const Transaction &txn, uint64_t ballot, client_preaccept_callback pcb) {

	Debug("[shard %i] Sending PREACCEPT [%lu]", shard, client_id);

	// create PREACCEPT Request
	string request_str;
	Request request;
	// PreAcceptMessage payload;
	request.set_op(Request::PREACCEPT);

	// serialize a Transaction into a TransactionMessage
	txn.serialize(request.mutable_preaccept()->mutable_txn());
	request.mutable_preaccept()->set_ballot(ballot);

	// now we can serialize the request and send it to replicas
	request.SerializeToString(&request_str);

	// ShardClient callback function will be able to invoke
	// the Client's callback function when all responses returned
	client->InvokeUnlogged(replica, request_str,
		std::bind(&ShardClient::PreAcceptCallback,
		placeholders::_1, placeholders::_2, placeholders::_3, pcb), nullptr); // no timeout case
}

void ShardClient::Accept(uint64_t txn_id, vector<string> deps, uint64_t ballot, client_accept_callback acb) {

	// TODO implement
	Debug("[shard %i] Sending ACCEPT [%lu]", shard, client_id);
	return;
}

void ShardClient::Commit(uint64_t txn_id, vector<string> deps, client_commit_callback ccb) {

	Debug("[shard %i] Sending PREACCEPT [%lu]", shard, client_id);

	// create COMMIT Request
	string request_str;
	Request request;
	request.set_op(Request::COMMIT);
	// CommitMessage payload
	request.mutable_commit()->set_txn_id(txn_id);
	for (auto dep : deps) {
		request.mutable_commit()->mutable_dep()->add_txnid(dep);
	}

	request.SerializeToString(&request_str);

	client->InvokeUnlogged(replica, request_str,
		std::bind(&ShardClient::CommitCallback,
			placeholders::_1, placeholders::_2, ccb), nullptr);
}
}