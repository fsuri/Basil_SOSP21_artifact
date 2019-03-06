#include "store/common/frontend/asynctransaction.h"

AsyncTransaction::AsyncTransaction() : opCount(0UL) {
}

AsyncTransaction::AsyncTransaction(const AsyncTransaction &txn) :
  opCount(txn.opCount), readValues(txn.readValues) {
}

AsyncTransaction::~AsyncTransaction() {
}

void AsyncTransaction::Execute(AsyncClient *client) {
  client->Begin();
  ExecuteNextOperation(client);
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

void AsyncTransaction::ExecuteNextOperation(AsyncClient *client) {
  size_t opId = opCount;
  ++opCount;
  OperationType type = GetNextOperationType();
  std::string key;
  std::string value;
  switch (type) {
    case GET:
      GetNextOperationKey(key);
      client->Get(key, [this, opId, client](int reply, const std::string &key,
          const std::string &value) {
        if (reply == REPLY_OK) {
          readValues.insert(std::make_pair(key, value));
          //GetOperation op = {opId, GET, key, value};
          GetOperation op(opId, GET, key, value);
          OnOperationCompleted(&op); 
          ExecuteNextOperation(client);
        }
      });
      break;
    case PUT:
      GetNextOperationKey(key); 
      GetNextPutValue(value);
      client->Put(key, value, [this, opId, client](int reply, const std::string &key) {
        //PutOperation op = {opId, PUT, key};
        if (reply == REPLY_OK) {
          PutOperation op(opId, PUT, key);
          OnOperationCompleted(&op); 
          ExecuteNextOperation(client);
        }
      });
      break;
    case COMMIT:
      client->Commit([this, opId, client](bool committed) {
        //CommitAbortOperation op = {opId, COMMIT, committed};
        CommitAbortOperation op(opId, COMMIT, committed);
        OnOperationCompleted(&op); 
        ExecuteNextOperation(client);
      });
      break;
    case ABORT:
      client->Abort([this, opId, client]() {
        //CommitAbortOperation op = {opId, ABORT, false};
        CommitAbortOperation op(opId, ABORT, false);
        OnOperationCompleted(&op); 
        ExecuteNextOperation(client);
      });
      break;
    default:
      // shouldnt get here
      break;
  }
}
