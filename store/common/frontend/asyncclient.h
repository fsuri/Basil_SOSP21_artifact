// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * common/client.h:
 *   Interface for a multiple shard transactional client.
 *
 **********************************************************************/

#ifndef _ASYNC_CLIENT_API_H_
#define _ASYNC_CLIENT_API_H_

#include "lib/assert.h"
#include "lib/message.h"

#include <functional>
#include <string>
#include <vector>

typedef std::function<void(const std::string &,
    const std::string &)> get_callback;

typedef std::function<void(const std::string &)> put_callback;

typedef std::function<void(bool)> commit_callback;

typedef std::function<void()> abort_callback;

class AsyncClient {
 public:
  AsyncClient() {}
  virtual ~AsyncClient() {}

  // Begin a transaction.
  virtual void Begin() = 0;

  // Get the value corresponding to key.
  virtual void Get(const std::string &key, get_callback cb) = 0;

  // Set the value for the given key.
  virtual void Put(const std::string &key, const std::string &value,
    put_callback cb) = 0;

  // Commit all Get(s) and Put(s) since Begin().
  virtual void Commit(commit_callback cb) = 0;
    
  // Abort all Get(s) and Put(s) since Begin().
  virtual void Abort(abort_callback cb) = 0;

  // Returns statistics (vector of integers) about most recent transaction.
  virtual std::vector<int> Stats() = 0;

};

#endif /* _ASYNC_CLIENT_API_H_ */
