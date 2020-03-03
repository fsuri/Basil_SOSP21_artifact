#include "store/janusstore/transaction.h"

using namespace std;

namespace janusstore {

Transaction::Transaction(uint64_t txn_id) {
	this->txn_id = txn_id;
}

Transaction::Transaction(uint64_t txn_id, const janusstore::proto::TransactionMessage &msg) {
	this->txn_id = txn_id;
	for (int i = 0; i < msg.groups_size(); i++) {
    this->groups.insert(msg.groups(i));
  }

  for (int i = 0; i < msg.gets_size(); i++) {
    this->addReadSet(msg.gets(i).key());
  }

  for (int i = 0; i < msg.puts_size(); i++) {
    proto::PutMessage put = msg.puts(i);
    this->addWriteSet(put.key(), put.value());
  }
}

Transaction::~Transaction() {}

void Transaction::setResult(std::unordered_map<std::string, std::string> result) {
	this->result = result;
}

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
	groups.insert(shard);
}
void Transaction::addShardedWriteSet(const std::string &key, const std::string &value, uint64_t shard) {
	sharded_writeset[shard][key] = value;
	groups.insert(shard);
}

void Transaction::serialize(janusstore::proto::TransactionMessage *msg, uint64_t shard) const {
	msg->set_status(this->status);
	msg->set_txnid(this->txn_id);

	// add readset for [shard]
	auto iter = sharded_readset.find(shard);
	if (iter != sharded_readset.end()) {
		for (auto &key : iter->second) {
	      msg->add_gets()->set_key(key);
	    }
	}

	// add writeset for [shard]
	auto iter2 = sharded_writeset.find(shard);
	if (iter2 != sharded_writeset.end()) {
	    for (auto &pair : iter2->second) {
	      janusstore::proto::PutMessage *put = msg->add_puts();
	      put->set_key(pair.first);
	      put->set_value(pair.second);
	    }
	}

	// add participating shards
	for (auto shard : groups) {
		msg->add_groups(shard);
	}
}

} // namespace janusstore
