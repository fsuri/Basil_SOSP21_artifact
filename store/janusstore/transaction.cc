#include "store/janusstore/transaction.h"

using namespace std;

namespace janusstore {

Transaction::Transaction(uint64_t txn_id, string server_ip, uint64_t server_port) {
	this->txn_id = txn_id;
	this->server_ip = server_ip;
	this->server_port = server_port;
}

Transaction::Transaction(uint64_t txn_id, string server_ip, uint64_t server_port, const TransactionMessage &msg) {
	this->txn_id = txn_id;
	this->server_ip = server_ip;
	this->server_port = server_port;
}

Transaction::~Transaction() {}

void Transaction::setTransactionId(uint64_t txn_id) {
	this->txn_id = txn_id;
}
void Transaction::setTransactionStatus(janusstore::proto::TransactionMessage::Status status) {
	this->status = status;
}
const uint64_t Transaction::getTransactionId() const {
	return txn_id;
}

const janusstore::proto::TransactionMessage::Status Transaction::getTransactionStatus() const {
	return status;
}
const std::unordered_set<std::string>& Transaction::getReadSet() const {
	return read_set;
}
const std::unordered_map<std::string, std::string>& Transaction::getWriteSet() const {
	return write_set;
}

void Transaction::addReadSet(const std::string &key) {
	read_set.insert(key);
}
void Transaction::addWriteSet(const std::string &key, const std::string &value){
	write_set[key] = value;
}
void Transaction::addShardedReadSet(const std::string &key, uint64_t shard) {
	sharded_readset[shard].insert(key);
}
void Transaction::addShardedWriteSet(const std::string &key, const std::string &value, uint64_t shard) {
	sharded_writeset[shard][key] = value;
}
void Transaction::serialize(janusstore::proto::TransactionMessage *msg, uint64_t shard) const {
	msg->set_status(this->status);
	msg->set_serverip(this->server_ip);
	msg->set_serverport(this->server_port);
	msg->set_txnid(this->txn_id);
	auto iter = sharded_readset.find(shard);
	ASSERT(iter != sharded_readset.end());
	for (auto &key : iter->second) {
      msg->add_gets()->set_key(key);
    }

	auto iter2 = sharded_writeset.find(shard);
	ASSERT(iter2 != sharded_writeset.end());
    for (auto &pair : iter2->second) {
      janusstore::proto::PutMessage *put = msg->add_puts();
      put->set_key(pair.first);
      put->set_value(pair.second);
    }
}

} // namespace janusstore
