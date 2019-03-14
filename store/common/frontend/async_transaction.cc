#include "store/common/frontend/async_transaction.h"

AsyncTransaction::AsyncTransaction(Client *client_) : client(client_),
    opCount(0UL) {
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
  client->Get(key,
      [this](int status, const std::string &key, const std::string &val,
            Timestamp ts){
        Debug("Get(%s) callback.", key.c_str());
        this->opCount++;
        this->readValues.insert(std::make_pair(key, val));
        this->ExecuteNextOperation();
      },
      [](int status, const std::string &key){
        Debug("Get(%s) timed out :(", key.c_str());
      },
      10000); // timeout doesn't really matter?
}

void AsyncTransaction::Put(const std::string &key,
    const std::string &value) {
  client->Put(key, value,
      [this](int status, const std::string &key, const std::string &value) {
        Debug("Put(%s,%s) callback.", key.c_str(), value.c_str());
        this->opCount++;
        this->ExecuteNextOperation();
      },
      [](int status, const std::string &key, const std::string &value){
        Debug("Put(%s,%s) timed out :(", key.c_str(), value.c_str());
      },
      10000); // timeout doesn't really matter?
}

void AsyncTransaction::Commit() {
  client->Commit(
      [this](bool committed){
        Debug("Commit callback.");
        this->currEcb(committed, this->readValues);
      },
      [](int status){
        Debug("Commit timed out :(");
      },
      10000); // timeout doesn't really matter?
}

void AsyncTransaction::Abort() {
  client->Abort([](){ }, [](int status){
        Debug("Abort timed out :(");
      },
      10000); // timeout doesn't really matter?
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
