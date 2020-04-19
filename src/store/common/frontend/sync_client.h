// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * common/client.h:
 *   Interface for a multiple shard transactional client.
 *
 **********************************************************************/

#ifndef _SYNC_CLIENT_API_H_
#define _SYNC_CLIENT_API_H_

#include "lib/assert.h"
#include "lib/message.h"
#include "store/common/timestamp.h"
#include "store/common/partitioner.h"
#include "store/common/frontend/client.h"
#include "store/common/promise.h"

#include <functional>
#include <string>
#include <vector>

class SyncClient {
 public:
  SyncClient(Client *client);
  virtual ~SyncClient();

  // Begin a transaction.
  virtual void Begin(uint32_t timeout);

  // Get the value corresponding to key.
  virtual void Get(const std::string &key, std::string &value,
      uint32_t timeout);

  // Set the value for the given key.
  virtual void Put(const std::string &key, const std::string &value,
      uint32_t timeout);

  // Commit all Get(s) and Put(s) since Begin().
  virtual int Commit(uint32_t timeout);
  
  // Abort all Get(s) and Put(s) since Begin().
  virtual void Abort(uint32_t timeout);

  // Returns statistics (vector of integers) about most recent transaction.
  std::vector<int> Stats();

 private:
  void GetCallback(Promise *promise, int status, const std::string &key, const std::string &value,
      Timestamp ts);
  void GetTimeoutCallback(Promise *promise, int status, const std::string &key);
  void PutCallback(Promise *promise, int status, const std::string &key,
      const std::string &value);
  void PutTimeoutCallback(Promise *promise, int status, const std::string &key,
      const std::string &value);
  void CommitCallback(Promise *promise, int status);
  void CommitTimeoutCallback(Promise *promise, int status);
  void AbortCallback(Promise *promise);
  void AbortTimeoutCallback(Promise *promise, int status);

  Client *client;
};

#endif /* _SYNC_CLIENT_API_H_ */
