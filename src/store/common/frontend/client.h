// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * common/client.h:
 *   Interface for a multiple shard transactional client.
 *
 **********************************************************************/

#ifndef _CLIENT_API_H_
#define _CLIENT_API_H_

#include "lib/assert.h"
#include "lib/message.h"
#include "store/common/timestamp.h"
#include "store/common/partitioner.h"

#include <functional>
#include <string>
#include <vector>

typedef std::function<void(uint64_t)> begin_callback;
typedef std::function<void()> begin_timeout_callback;

typedef std::function<void(int, const std::string &,
    const std::string &, Timestamp)> get_callback;
typedef std::function<void(int, const std::string &)> get_timeout_callback;

typedef std::function<void(int, const std::string &,
    const std::string &)> put_callback;
typedef std::function<void(int, const std::string &,
    const std::string &)> put_timeout_callback;

typedef std::function<void(int)> commit_callback;
typedef std::function<void(int)> commit_timeout_callback;

typedef std::function<void()> abort_callback;
typedef std::function<void(int)> abort_timeout_callback;

class Client {
 public:
  Client() {};
  virtual ~Client() {};

  // Begin a transaction.
  virtual void Begin(begin_callback bcb, begin_timeout_callback btcb,
      uint32_t timeout) = 0;

  // Get the value corresponding to key.
  virtual void Get(const std::string &key, get_callback gcb,
      get_timeout_callback gtcb, uint32_t timeout) = 0;

  // Set the value for the given key.
  virtual void Put(const std::string &key, const std::string &value,
      put_callback pcb, put_timeout_callback ptcb, uint32_t timeout) = 0;

  // Commit all Get(s) and Put(s) since Begin().
  virtual void Commit(commit_callback ccb, commit_timeout_callback ctcb,
      uint32_t timeout) = 0;
  
  // Abort all Get(s) and Put(s) since Begin().
  virtual void Abort(abort_callback acb, abort_timeout_callback atcb,
      uint32_t timeout) = 0;

  // Returns statistics (vector of integers) about most recent transaction.
  virtual std::vector<int> Stats() = 0;

};

#endif /* _CLIENT_API_H_ */
