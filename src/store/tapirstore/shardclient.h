// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * store/tapir/shardclient.h:
 *   Single shard tapir transactional client interface.
 *
 * Copyright 2015 Irene Zhang <iyzhang@cs.washington.edu>
 *                Naveen Kr. Sharma <naveenks@cs.washington.edu>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************************/

#ifndef _TAPIR_SHARDCLIENT_H_
#define _TAPIR_SHARDCLIENT_H_

#include "lib/assert.h"
#include "lib/configuration.h"
#include "lib/message.h"
#include "lib/transport.h"
#include "replication/ir/client.h"
#include "store/common/timestamp.h"
#include "store/common/transaction.h"
#include "store/common/frontend/txnclient.h"
#include "store/tapirstore/tapir-proto.pb.h"

#include <map>
#include <string>

namespace tapirstore {

class ShardClient : public TxnClient {
 public:
  /* Constructor needs path to shard config. */
  ShardClient(transport::Configuration *config, Transport *transport,
      uint64_t client_id, int shard, int closestReplica);
  virtual ~ShardClient();

  // Begin a transaction.
  virtual void Begin(uint64_t id) override;

  // Get the value corresponding to key.
  virtual void Get(uint64_t id, const std::string &key, get_callback gcb,
      get_timeout_callback gtcb, uint32_t timeout) override;
  virtual void Get(uint64_t id, const std::string &key,
      const Timestamp &timestamp, get_callback gcb, get_timeout_callback gtcb,
      uint32_t timeout) override;

  // Set the value for the given key.
  virtual void Put(uint64_t id, const std::string &key,
      const std::string &value, put_callback pcb, put_timeout_callback ptcb,
      uint32_t timeout) override;

  // Commit all Get(s) and Put(s) since Begin().
  virtual void Commit(uint64_t id, const Transaction & txn,
      const Timestamp &timestamp, commit_callback ccb, commit_timeout_callback ctcb,
      uint32_t timeout) override;
  
  // Abort all Get(s) and Put(s) since Begin().
  virtual void Abort(uint64_t id, const Transaction &txn,
      abort_callback acb, abort_timeout_callback atcb,
      uint32_t timeout) override;

  // Prepare the transaction.
  virtual void Prepare(uint64_t id, const Transaction &txn,
      const Timestamp &timestamp, prepare_callback pcb,
      prepare_timeout_callback ptcb, uint32_t timeout) override;

 private:
  struct PendingPrepare : public TxnClient::PendingPrepare {
    PendingPrepare(uint64_t reqId) : TxnClient::PendingPrepare(reqId),
        requestTimeout(nullptr) { }
    ~PendingPrepare() {
      if (requestTimeout != nullptr) {
        delete requestTimeout;
      }
    }
    Timestamp ts;
    Transaction txn;
    Timeout *requestTimeout;
  };
  struct PendingCommit : public TxnClient::PendingCommit {
    PendingCommit(uint64_t reqId) : TxnClient::PendingCommit(reqId),
        requestTimeout(nullptr) { }
    ~PendingCommit() {
      if (requestTimeout != nullptr) {
        delete requestTimeout;
      }
    }
    Timestamp ts;
    Transaction txn;
    Timeout *requestTimeout;
  };
  struct PendingAbort : public TxnClient::PendingAbort {
    PendingAbort(uint64_t reqId) : TxnClient::PendingAbort(reqId),
        requestTimeout(nullptr) { }
    ~PendingAbort() {
      if (requestTimeout != nullptr) {
        delete requestTimeout;
      }
    }
    Transaction txn;
    Timeout *requestTimeout;
  };

  uint64_t client_id; // Unique ID for this client.
  Transport *transport; // Transport layer.
  transport::Configuration *config;
  int shard; // which shard this client accesses
  int replica; // which replica to use for reads

  replication::ir::IRClient *client; // Client proxy.

  std::unordered_map<uint64_t, PendingGet *> pendingGets;
  std::unordered_map<uint64_t, PendingPrepare *> pendingPrepares;
  std::unordered_map<uint64_t, PendingCommit *> pendingCommits;
  std::unordered_map<uint64_t, PendingAbort *> pendingAborts;

  /* Tapir's Decide Function. */
  std::string TapirDecide(const std::map<std::string, std::size_t> &results);

  /* Timeout for Get requests, which only go to one replica. */
  void GetTimeout(uint64_t id, uint64_t reqId);

  /* Callbacks for hearing back from a shard for an operation. */
  bool GetCallback(uint64_t reqId, const std::string &,
      const std::string &);
  bool PrepareCallback(uint64_t reqId, const std::string &,
      const std::string &);
  bool CommitCallback(uint64_t reqId, const std::string &,
      const std::string &);
  bool AbortCallback(uint64_t reqId, const std::string &,
      const std::string &);

  /* Helper Functions for starting and finishing requests */
  void StartRequest();
  void WaitForResponse();
  void FinishRequest(const std::string &reply_str);
  void FinishRequest();
  int SendGet(const std::string &request_str);
};

} // namespace tapirstore

#endif /* _TAPIR_SHARDCLIENT_H_ */
