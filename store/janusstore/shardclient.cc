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

void ShardClient::PreAccept(const Transaction &txn, uint64_t ballot, client_preaccept_callback pcb) {
	Debug("[shard %i] Sending PREACCEPT [%llu]", shard, client_id);

	std::vector<janusstore::proto::Reply> replies;
	uint64_t txn_id = txn.getTransactionId();
	pair<uint64_t, std::vector<janusstore::proto::Reply>> entry (txn_id, replies);
	preaccept_replies.insert(entry);

	// create PREACCEPT Request
	string request_str;
	Request request;
	// PreAcceptMessage payload;
	request.set_op(Request::PREACCEPT);

	// serialize a Transaction into a TransactionMessage
	janusstore::proto::TransactionMessage txn_msg;
	txn.serialize(&txn_msg);
	// printf("%s", txn_msg->DebugString().c_str());
	request.mutable_preaccept()->set_allocated_txn(&txn_msg);
	// txn.serialize(request.mutable_preaccept()->mutable_txn());
	request.mutable_preaccept()->set_ballot(ballot);


	// now we can serialize the request and send it to replicas
	request.SerializeToString(&request_str);
	request.mutable_preaccept()->release_txn();

	// store callback with txnid in a map for the preaccept cb
	// because we can't pass it into a continuation function
	pair<uint64_t, client_preaccept_callback> callback_entry (txn_id, pcb);
	this->pcb_map.insert(callback_entry);

	// ShardClient continutation will be able to invoke
	// the Client's callback function when all responses returned
	client->InvokeUnlogged(replica, request_str,
		std::bind(&ShardClient::PreAcceptContinuation, this,
		placeholders::_1, placeholders::_2), nullptr, 0);
		// no timeout case; TODO verify 0 is OK
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

	// store callback with txnid in a map for the preaccept cb
	// because we can't pass it into a continuation function
	pair<uint64_t, client_accept_callback> callback_entry (txn_id, acb);
	this->acb_map.insert(callback_entry);

	// ShardClient continutation will be able to invoke
	// the Client's callback function when all responses returned
	client->InvokeUnlogged(replica, request_str,
		std::bind(&ShardClient::AcceptContinuation, this,
		placeholders::_1, placeholders::_2), nullptr, 0);
		// no timeout case; TODO verify 0 is OK
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

	// store callback with txnid in a map for the preaccept cb
	// because we can't pass it into a continuation function
	pair<uint64_t, client_commit_callback> callback_entry (txn_id, ccb);
	this->ccb_map.insert(callback_entry);

	// ShardClient continutation will be able to invoke
	// the Client's callback function when all responses returned
	client->InvokeUnlogged(replica, request_str,
		std::bind(&ShardClient::CommitContinuation, this,
		placeholders::_1, placeholders::_2), nullptr, 0);
		// no timeout case; TODO verify 0 is OK
}

void ShardClient::PreAcceptCallback(uint64_t txn_id, janusstore::proto::Reply reply, client_preaccept_callback pcb) {
	// TODO who unwraps replica responses into the proto?
	this->responded++;

	// aggregate replies for this transaction
	if (this->preaccept_replies.count(txn_id)) {
		// key already exists, so append to list
		std::vector<janusstore::proto::Reply> list;
		list = this->preaccept_replies.at(txn_id);
		list.push_back(reply);
	} else {
		this->preaccept_replies[txn_id] = std::vector<janusstore::proto::Reply>({reply});
	}

	// TODO how do determine number of replicas?
	if (this->responded) {
		this->responded = 0;
		pcb(shard, this->preaccept_replies[txn_id]);
	}
}

void ShardClient::AcceptCallback(uint64_t txn_id, janusstore::proto::Reply reply, client_accept_callback acb) {
	// TODO who unwraps replica responses into the callback params?
	responded++;

	// aggregate replies for this transaction
	if (this->accept_replies.count(txn_id)) {
		// key already exists, so append to list
		// key already exists, so append to list
		std::vector<janusstore::proto::Reply> list;
		list = this->accept_replies.at(txn_id);
		list.push_back(reply);
	} else {
		this->accept_replies[txn_id] = std::vector<janusstore::proto::Reply>({reply});
	}

	// TODO how do determine number of replicas?
	if (responded) {
		responded = 0;
		acb(shard, this->accept_replies[txn_id]);
	}
}

void ShardClient::CommitCallback(uint64_t txn_id, janusstore::proto::Reply reply, client_commit_callback ccb) {
	// TODO who unwraps replica responses into the callback params?
	responded++;

	// aggregate replies for this transaction
	if (this->commit_replies.count(txn_id)) {
		// key already exists, so append to list
		// key already exists, so append to list
		std::vector<janusstore::proto::Reply> list;
		list = this->commit_replies.at(txn_id);
		list.push_back(reply);
	} else {
		this->commit_replies[txn_id] = std::vector<janusstore::proto::Reply>({reply});
	}

	// TODO how do determine number of replicas?
	if (responded) {
		responded = 0;
		ccb(shard, this->commit_replies[txn_id]);
	}
}

void ShardClient::PreAcceptContinuation(const string &request_str, const string &reply_str) {

	Debug("In preaccept continuation");
	janusstore::proto::Reply reply;
	reply.ParseFromString(reply_str);
	uint64_t txn_id = NULL;

	if (reply.op() == janusstore::proto::Reply::PREACCEPT_OK) {
		txn_id = reply.preaccept_ok().txnid();
	} else if (reply.op() == janusstore::proto::Reply::PREACCEPT_NOT_OK) {
		txn_id = reply.preaccept_not_ok().txnid();
	} else {
		Panic("Not a preaccept reply");
	}

	// get the pcb
  	client_preaccept_callback pcb = this->pcb_map.at(txn_id);
  	// invoke the shardclient callback
  	this->PreAcceptCallback(txn_id, reply, pcb);
}

void ShardClient::AcceptContinuation(const string &request_str,
    const string &reply_str) {

	Debug("In accept continuation");
	janusstore::proto::Reply reply;
  	reply.ParseFromString(reply_str);
	uint64_t txn_id = NULL;

	if (reply.op() == janusstore::proto::Reply::ACCEPT_OK) {
		txn_id = reply.accept_ok().txnid();
	} else if (reply.op() == janusstore::proto::Reply::ACCEPT_NOT_OK) {
		txn_id = reply.accept_not_ok().txnid();
	} else {
		Panic("Not an accept reply");
	}
  	// get the acb
  	client_accept_callback acb = this->acb_map.at(txn_id);
  	// invoke the shardclient callback
  	this->AcceptCallback(txn_id, reply, acb);
}

void ShardClient::CommitContinuation(const string &request_str,
    const string &reply_str) {

	Debug("In commit continuation");
	janusstore::proto::Reply reply;
  	reply.ParseFromString(reply_str);
	uint64_t txn_id = NULL;

	if (reply.op() == janusstore::proto::Reply::COMMIT_OK) {
		txn_id = reply.commit_ok().txnid();
	} else {
		Panic("Not a commit reply");
	}
  	// get the ccb
  	client_commit_callback ccb = this->ccb_map.at(txn_id);
  	// invoke the shardclient callback
  	this->CommitCallback(txn_id, reply, ccb);
}
}
