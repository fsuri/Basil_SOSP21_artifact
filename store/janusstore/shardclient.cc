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

void ShardClient::PreAccept(uint64_t id, const Transaction &txn, uint64_t ballot, preaccept_callback pcb) {

	Debug("[shard %i] Sending PREACCEPT [%lu]", shard, id);

	// create PREACCEPT Request
	string request_str;
	Request request;
	// PreAcceptMessage payload;
	request.set_op(Request::PREACCEPT);

	// serialize a Transaction into a TransactionMessage
	txn.serialize(request.mutable_preaccept()->mutable_txn());
	request.mutable_preaccept()->ballot = ballot;

	// now we can serialize the request and send it to replicas
	request.SerializeToString(&request_str);

	// TODO is this ok?
	client->InvokeUnlogged(replica, request_str,
		bind(&ShardClient::PreAcceptCallback, this, placeholders::_1, placeholders::_2, placeholders::_3), nullptr); // no error case
}
}