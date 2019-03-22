#include "store/common/frontend/async_transaction.h"

#include "lib/latency.h"

AsyncTransaction::AsyncTransaction(uint64_t tid, Client *client_) : 
    tid(tid), client(client_), opCount(0UL) {
}

AsyncTransaction::AsyncTransaction(const AsyncTransaction &txn) :
  client(txn.client), opCount(txn.opCount), readValues(txn.readValues) {
}

AsyncTransaction::~AsyncTransaction() {
}

void AsyncTransaction::Execute(execute_callback ecb) {
  currEcb = ecb;
  client->Begin();
  ExecuteNextOperation();
}

void AsyncTransaction::Get(const std::string &key) {
  client->Get(key, std::bind(&AsyncTransaction::GetCallback, this,
        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
        std::placeholders::_4), std::bind(&AsyncTransaction::GetTimeout, this,
          std::placeholders::_1, std::placeholders::_2), 10000);
  // timeout doesn't really matter?
}

void AsyncTransaction::Put(const std::string &key,
    const std::string &value) {
  client->Put(key, value, std::bind(&AsyncTransaction::PutCallback, this,
        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
      std::bind(&AsyncTransaction::PutTimeout, this, std::placeholders::_1,
        std::placeholders::_2, std::placeholders::_3), 10000);
  // timeout doesn't really matter?
}

void AsyncTransaction::Commit() {
  client->Commit(std::bind(&AsyncTransaction::CommitCallback, this,
        std::placeholders::_1),
      std::bind(&AsyncTransaction::CommitTimeout, this, std::placeholders::_1),
      10000);
  // timeout doesn't really matter?
}

void AsyncTransaction::Abort() {
  client->Abort(std::bind(&AsyncTransaction::AbortCallback, this),
      std::bind(&AsyncTransaction::AbortTimeout, this, std::placeholders::_1),
      10000);
  // timeout doesn't really matter?
  currEcb(false, std::map<std::string, std::string>());
}

void AsyncTransaction::GetReadValue(const std::string &key,
    std::string &value, bool &found) const {
  auto itr = readValues.find(key);
  if (itr == readValues.end()) {
    found = false;
  } else {
    found = true;
    // copy here
    value = itr->second;
  }
}

void AsyncTransaction::GetCallback(int status, const std::string &key,
    const std::string &val, Timestamp ts) {
  Debug("Get(%s) callback.", key.c_str());
  opCount++;
  readValues.insert(std::make_pair(key, val));
  ExecuteNextOperation();
}

void AsyncTransaction::GetTimeout(int status, const std::string &key) {
  Debug("Get(%s) timed out :(", key.c_str());
}

void AsyncTransaction::PutCallback(int status, const std::string &key,
    const std::string &val) {
  Debug("Put(%s,%s) callback.", key.c_str(), val.c_str());
  opCount++;
  ExecuteNextOperation();
}

void AsyncTransaction::PutTimeout(int status, const std::string &key,
    const std::string &val) {
  Debug("Put(%s,%s) timed out :(", key.c_str(), val.c_str());
}

void AsyncTransaction::CommitCallback(bool committed) {
  Debug("Commit callback.");
  currEcb(committed, readValues);
}

void AsyncTransaction::CommitTimeout(int status) {
  Debug("Commit timed out :(");
}

void AsyncTransaction::AbortCallback() {
  Debug("Abort callback.");
}

void AsyncTransaction::AbortTimeout(int status) {
  Debug("Abort timed out :(");
}

