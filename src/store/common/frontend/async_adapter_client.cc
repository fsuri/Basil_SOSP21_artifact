#include "store/common/frontend/async_adapter_client.h"

AsyncAdapterClient::AsyncAdapterClient(Client *client, uint32_t timeout) :
    client(client), timeout(timeout), outstandingOpCount(0UL), finishedOpCount(0UL) {
}

AsyncAdapterClient::~AsyncAdapterClient() {
}

void AsyncAdapterClient::Execute(AsyncTransaction *txn,
    execute_callback ecb) {
  currEcb = ecb;
  currTxn = txn;
  outstandingOpCount = 0UL;
  finishedOpCount = 0UL;
  readValues.clear();
  client->Begin([this](uint64_t id) {
    ExecuteNextOperation();
  }, []{}, timeout);
}

void AsyncAdapterClient::ExecuteNextOperation() {
  Operation op = currTxn->GetNextOperation(outstandingOpCount, finishedOpCount,
      readValues);
  switch (op.type) {
    case GET: {
      client->Get(op.key, std::bind(&AsyncAdapterClient::GetCallback, this,
        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
        std::placeholders::_4), std::bind(&AsyncAdapterClient::GetTimeout, this,
          std::placeholders::_1, std::placeholders::_2), timeout);
      ++outstandingOpCount;
      // timeout doesn't really matter?
      ExecuteNextOperation();
      break;
    }
    case PUT: {
      client->Put(op.key, op.value, std::bind(&AsyncAdapterClient::PutCallback,
            this, std::placeholders::_1, std::placeholders::_2,
            std::placeholders::_3), std::bind(&AsyncAdapterClient::PutTimeout,
              this, std::placeholders::_1, std::placeholders::_2,
              std::placeholders::_3), timeout);
      ++outstandingOpCount;
      // timeout doesn't really matter?
      ExecuteNextOperation();
      break;
    }
    case COMMIT: {
      client->Commit(std::bind(&AsyncAdapterClient::CommitCallback, this,
        std::placeholders::_1), std::bind(&AsyncAdapterClient::CommitTimeout,
          this), timeout);
      // timeout doesn't really matter?
      break;
    }
    case ABORT: {
      client->Abort(std::bind(&AsyncAdapterClient::AbortCallback, this),
          std::bind(&AsyncAdapterClient::AbortTimeout, this), timeout);
      // timeout doesn't really matter?
      currEcb(ABORTED_USER, std::map<std::string, std::string>());
      break;
    }
    case WAIT:
      break;
    default:
      NOT_REACHABLE();
  }
}

void AsyncAdapterClient::GetCallback(int status, const std::string &key,
    const std::string &val, Timestamp ts) {
  Debug("Get(%s) callback.", key.c_str());
  readValues.insert(std::make_pair(key, val));
  finishedOpCount++;
  ExecuteNextOperation();
}

void AsyncAdapterClient::GetTimeout(int status, const std::string &key) {
  Warning("Get(%s) timed out :(", key.c_str());
  outstandingOpCount--;
}

void AsyncAdapterClient::PutCallback(int status, const std::string &key,
    const std::string &val) {
  Debug("Put(%s,%s) callback.", key.c_str(), val.c_str());
  finishedOpCount++;
  ExecuteNextOperation();
}

void AsyncAdapterClient::PutTimeout(int status, const std::string &key,
    const std::string &val) {
  Warning("Put(%s,%s) timed out :(", key.c_str(), val.c_str());
  outstandingOpCount--;
}

void AsyncAdapterClient::CommitCallback(transaction_status_t result) {
  Debug("Commit callback.");
  currEcb(result, readValues);
}

void AsyncAdapterClient::CommitTimeout() {
  Warning("Commit timed out :(");
}

void AsyncAdapterClient::AbortCallback() {
  Debug("Abort callback.");
}

void AsyncAdapterClient::AbortTimeout() {
  Warning("Abort timed out :(");
}

