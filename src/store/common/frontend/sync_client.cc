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

int SyncClient::Commit(uint32_t timeout) {
  Promise promise(timeout);

  client->Commit(std::bind(&SyncClient::CommitCallback, this, &promise,
        std::placeholders::_1),
        std::bind(&SyncClient::CommitTimeoutCallback, this,
        &promise, std::placeholders::_1), timeout);

  return promise.GetReply();
}
  
void SyncClient::Abort(uint32_t timeout) {
  Promise promise(timeout);

  client->Abort(std::bind(&SyncClient::AbortCallback, this, &promise),
        std::bind(&SyncClient::AbortTimeoutCallback, this, &promise,
          std::placeholders::_1), timeout);

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

void SyncClient::CommitCallback(Promise *promise, int status) {
  promise->Reply(status);
}

void SyncClient::CommitTimeoutCallback(Promise *promise, int status) {
  promise->Reply(status);
}

void SyncClient::AbortCallback(Promise *promise) {
  promise->Reply(REPLY_FAIL);
}

void SyncClient::AbortTimeoutCallback(Promise *promise, int status) {
  promise->Reply(status);
}

