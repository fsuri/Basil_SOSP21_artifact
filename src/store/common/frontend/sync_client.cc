#include "store/common/frontend/sync_client.h"


SyncClient::SyncClient(Client *client) : client(client) {
}

SyncClient::~SyncClient() {
}

void SyncClient::Begin(uint32_t timeout) {
  Promise promise(timeout);
  client->Begin([promisePtr = &promise](uint64_t id){ promisePtr->Reply(0); },
      [](){}, timeout);
  promise.GetReply();
}

void SyncClient::Get(const std::string &key, std::string &value,
      uint32_t timeout) {
  Promise promise(timeout);
  client->Get(key, std::bind(&SyncClient::GetCallback, this, &promise,
        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
        std::placeholders::_4), std::bind(&SyncClient::GetTimeoutCallback, this,
        &promise, std::placeholders::_1, std::placeholders::_2), timeout);
  value = promise.GetValue();
}

void SyncClient::Get(const std::string &key, uint32_t timeout) {
  Promise *promise = new Promise(timeout);
  getPromises.push_back(promise);
  client->Get(key, std::bind(&SyncClient::GetCallback, this, promise,
      std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
      std::placeholders::_4), std::bind(&SyncClient::GetTimeoutCallback, this,
      promise, std::placeholders::_1, std::placeholders::_2), timeout);
}

void SyncClient::Wait(std::vector<std::string> &values) {
  for (auto promise : getPromises) {
    values.push_back(promise->GetValue());
    delete promise;
  }
  getPromises.clear();
}

void SyncClient::Put(const std::string &key, const std::string &value,
      uint32_t timeout) {
  Promise promise(timeout);

  client->Put(key, value, std::bind(&SyncClient::PutCallback, this, &promise,
        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
        std::bind(&SyncClient::PutTimeoutCallback, this,
        &promise, std::placeholders::_1, std::placeholders::_2,
        std::placeholders::_3), timeout);

  promise.GetReply();
}

transaction_status_t SyncClient::Commit(uint32_t timeout) {
  if (getPromises.size() > 0) {
    std::vector<std::string> strs;
    Wait(strs);
  }

  Promise promise(timeout);

  client->Commit(std::bind(&SyncClient::CommitCallback, this, &promise,
        std::placeholders::_1),
        std::bind(&SyncClient::CommitTimeoutCallback, this,
        &promise), timeout);

  return static_cast<transaction_status_t>(promise.GetReply());
}
  
void SyncClient::Abort(uint32_t timeout) {
  if (getPromises.size() > 0) {
    std::vector<std::string> strs;
    Wait(strs);
  }

  Promise promise(timeout);

  client->Abort(std::bind(&SyncClient::AbortCallback, this, &promise),
        std::bind(&SyncClient::AbortTimeoutCallback, this, &promise), timeout);

  promise.GetReply();
}

void SyncClient::GetCallback(Promise *promise, int status,
    const std::string &key, const std::string &value, Timestamp ts){
  promise->Reply(status, ts, value);
}

void SyncClient::GetTimeoutCallback(Promise *promise, int status, const std::string &key) {
  promise->Reply(status);
}

void SyncClient::PutCallback(Promise *promise, int status, const std::string &key,
      const std::string &value) {
  promise->Reply(status);
}

void SyncClient::PutTimeoutCallback(Promise *promise, int status, const std::string &key,
      const std::string &value) {
  promise->Reply(status);
}

void SyncClient::CommitCallback(Promise *promise, transaction_status_t status) {
  promise->Reply(status);
}

void SyncClient::CommitTimeoutCallback(Promise *promise) {
  promise->Reply(REPLY_TIMEOUT);
}

void SyncClient::AbortCallback(Promise *promise) {
  promise->Reply(ABORTED_USER);
}

void SyncClient::AbortTimeoutCallback(Promise *promise) {
  promise->Reply(REPLY_TIMEOUT);
}

