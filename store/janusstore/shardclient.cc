// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
#include "store/janusstore/shardclient.h"

namespace janusstore {

using namespace std;
using namespace proto;

ShardClient::ShardClient(const string &configPath, Transport *transport,
	uint64_t client_id, int shard, int closestReplica)
	: client_id(client_id), transport(transport), shard(shard), responded(0) {
  
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
*/

void ShardClient::PreAccept(const Transaction &txn, uint64_t ballot, client_preaccept_callback pcb) {
	Debug("[shard %i] Sending PREACCEPT [%llu]", shard, client_id);

	std::vector<janusstore::proto::Reply> replies;
	pair<uint64_t, std::vector<janusstore::proto::Reply>> entry (txn.getTransactionId(), replies);
	preaccept_replies.insert(entry);

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

	// TODO store callback with txnid in a map for the preaccept cb

	// ShardClient callback function will be able to invoke
	// the Client's callback function when all responses returned
	// TODO use the continuation callbacks instead
	/*client->InvokeUnlogged(replica, request_str,
		std::bind(&ShardClient::PreAcceptCallback,
		txn.getTransactionId(), placeholders::_2, pcb),
    std::bind(&ShardClient::PreAcceptTimeout, this), 0); // no timeout case*/
}

void ShardClient::Accept(uint64_t txn_id, std::vector<uint64_t> deps, uint64_t ballot, client_accept_callback acb) {
	Debug("[shard %i] Sending ACCEPT [%llu]", shard, client_id);

	std::vector<janusstore::proto::Reply> replies;
	pair<uint64_t, std::vector<janusstore::proto::Reply>> entry (txn_id, replies);
	accept_replies.insert(entry);

	// create ACCEPT Request
	string request_str;
	Request request;
	request.set_op(Request::ACCEPT);

	// AcceptMessage payload
	request.mutable_accept()->set_txnid(txn_id);
	request.mutable_accept()->set_ballot(ballot);
	for (auto dep : deps) {
		request.mutable_accept()->mutable_dep()->add_txnid(dep);
	}
	
	request.SerializeToString(&request_str);

	// TODO store callback with txnid in a map for the preaccept cb
	// TODO use the continuation callbacks instead
	/*client->InvokeUnlogged(replica, request_str,
		std::bind(&ShardClient::AcceptCallback,
			txn_id, placeholders::_2, acb), std::bind(&ShardClient::AcceptTimeout, this), 0);*/
}

void ShardClient::Commit(uint64_t txn_id, std::vector<uint64_t> deps, client_commit_callback ccb) {
	Debug("[shard %i] Sending COMMIT [%llu]", shard, client_id);

	std::vector<janusstore::proto::Reply> replies;
	pair<uint64_t, std::vector<janusstore::proto::Reply>> entry (txn_id, replies);
	commit_replies.insert(entry);

	// create COMMIT Request
	string request_str;
	Request request;
	request.set_op(Request::COMMIT);
	// CommitMessage payload
	request.mutable_commit()->set_txnid(txn_id);
	for (auto dep : deps) {
		request.mutable_commit()->mutable_dep()->add_txnid(dep);
	}

	request.SerializeToString(&request_str);

	// TODO store callback with txnid in a map for the preaccept cb
	// TODO use the continuation callbacks instead
	/*client->InvokeUnlogged(replica, request_str,
		std::bind(&ShardClient::CommitCallback, this,
			txn_id, placeholders::_2, ccb),
      std::bind(&ShardClient::CommitTimeout, this), 0);*/
}

void ShardClient::PreAcceptCallback(uint64_t txn_id, janusstore::proto::Reply reply, client_preaccept_callback pcb) {
	// TODO who unwraps replica responses into the proto?
	responded++;

	// aggregate replies for this transaction
	preaccept_replies[txn_id].push_back(reply);

	// TODO how do determine number of replicas?
	if (responded) {
		responded = 0;
		pcb(shard, preaccept_replies[txn_id]);
	}
}

void ShardClient::AcceptCallback(uint64_t txn_id, std::vector<janusstore::proto::Reply> replies, client_accept_callback acb) {
	// TODO who unwraps replica responses into the callback params?
	responded++;
	
	// aggregate replies for this transaction
	//accept_replies[txn_id].insert(reply);

	// TODO how do determine number of replicas?
	if (responded) {
		responded = 0;
		acb(shard, accept_replies[txn_id]);
	}
}

void ShardClient::CommitCallback(uint64_t txn_id, janusstore::proto::Reply reply, client_commit_callback ccb) {
	// TODO who unwraps replica responses into the callback params?
	responded++;
	
	// aggregate replies for this transaction
	commit_replies[txn_id].push_back(reply);

	// TODO how do determine number of replicas?
	if (responded) {
		responded = 0;
		ccb(shard, commit_replies[txn_id]);
	}
}

void ShardClient::PreAcceptContinuation() {

}

void ShardClient::AcceptContinuation() {
	
}

void ShardClient::CommitContinuation() {
	
}

void ShardClient::PreAcceptTimeout() {
}
void ShardClient::AcceptTimeout() {
}
void ShardClient::CommitTimeout() {
}

}
