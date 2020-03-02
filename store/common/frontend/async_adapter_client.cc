#include "store/common/frontend/async_adapter_client.h"

AsyncAdapterClient::AsyncAdapterClient(Client *client) : client(client),
    opCount(0UL) {
}

AsyncAdapterClient::~AsyncAdapterClient() {
}

void AsyncAdapterClient::Execute(AsyncTransaction *txn,
    execute_callback ecb) {
  currEcb = ecb;
  currTxn = txn;
  opCount = 0UL;
  readValues.clear();
  client->Begin();
  ExecuteNextOperation();
}

void AsyncAdapterClient::ExecuteNextOperation() {
  Operation op = currTxn->GetNextOperation(opCount, readValues);
  switch (op.type) {
    case GET: {
      client->Get(op.key, std::bind(&AsyncAdapterClient::GetCallback, this,
        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
        std::placeholders::_4), std::bind(&AsyncAdapterClient::GetTimeout, this,
          std::placeholders::_1, std::placeholders::_2), 10000);
      // timeout doesn't really matter?
      break;
    }
    case PUT: {
      client->Put(op.key, op.value, std::bind(&AsyncAdapterClient::PutCallback,
            this, std::placeholders::_1, std::placeholders::_2,
            std::placeholders::_3), std::bind(&AsyncAdapterClient::PutTimeout,
              this, std::placeholders::_1, std::placeholders::_2,
              std::placeholders::_3), 10000);
      // timeout doesn't really matter?
      break;
    }
    case COMMIT: {
      client->Commit(std::bind(&AsyncAdapterClient::CommitCallback, this,
        std::placeholders::_1), std::bind(&AsyncAdapterClient::CommitTimeout,
          this, std::placeholders::_1), 10000);
      // timeout doesn't really matter?
      break;
    }
    case ABORT: {
      client->Abort(std::bind(&AsyncAdapterClient::AbortCallback, this),
          std::bind(&AsyncAdapterClient::AbortTimeout, this,
            std::placeholders::_1), 10000);
      // timeout doesn't really matter?
      currEcb(false, std::map<std::string, std::string>());
      break;
    }
    default:
      NOT_REACHABLE();
  }
}

void AsyncAdapterClient::GetCallback(int status, const std::string &key,
    const std::string &val, Timestamp ts) {
  Debug("Get(%s) callback.", key.c_str());
  opCount++;
  readValues.insert(std::make_pair(key, val));
  ExecuteNextOperation();
}

void AsyncAdapterClient::GetTimeout(int status, const std::string &key) {
  Debug("Get(%s) timed out :(", key.c_str());
}

void AsyncAdapterClient::PutCallback(int status, const std::string &key,
    const std::string &val) {
  Debug("Put(%s,%s) callback.", key.c_str(), val.c_str());
  opCount++;
  ExecuteNextOperation();
}

void AsyncAdapterClient::PutTimeout(int status, const std::string &key,
    const std::string &val) {
  Debug("Put(%s,%s) timed out :(", key.c_str(), val.c_str());
}

void AsyncAdapterClient::CommitCallback(int result) {
  Debug("Commit callback.");
  currEcb(result, readValues);
}

void AsyncAdapterClient::CommitTimeout(int status) {
  Debug("Commit timed out :(");
}

void AsyncAdapterClient::AbortCallback() {
  Debug("Abort callback.");
}

void AsyncAdapterClient::AbortTimeout(int status) {
  Debug("Abort timed out :(");
}

