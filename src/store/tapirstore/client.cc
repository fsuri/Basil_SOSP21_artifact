// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * store/tapirstore/client.cc:
 *   Client to TAPIR transactional storage system.
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

#include "store/tapirstore/client.h"

namespace tapirstore {

using namespace std;

Client::Client(transport::Configuration *config, uint64_t id, int nShards,
    int nGroups, int closestReplica, Transport *transport, partitioner part,
    bool syncCommit, TrueTime timeServer) : config(config), client_id(id),
    nshards(nShards), ngroups(nGroups), transport(transport), part(part),
    syncCommit(syncCommit), timeServer(timeServer), lastReqId(0UL) {
    t_id = client_id << 32;

    bclient.reserve(nshards);

    Debug("Initializing Tapir client with id [%lu] %lu", client_id, nshards);

    /* Start a client for each shard. */
    for (uint64_t i = 0; i < ngroups; i++) {
        ShardClient *shardclient = new ShardClient(config,
                transport, client_id, i, closestReplica);
        bclient[i] = new BufferClient(shardclient);
    }

    Debug("Tapir client [%lu] created! %lu %lu", client_id, nshards, bclient.size());
}

Client::~Client()
{
    for (auto b : bclient) {
        delete b;
    }
}

/* Begins a transaction. All subsequent operations before a commit() or
 * abort() are part of this transaction.
 *
 * Return a TID for the transaction.
 */
void Client::Begin(begin_callback bcb, begin_timeout_callback btcb,
      uint32_t timeout) {
  transport->Timer(0, [this, bcb, btcb, timeout]() {
    Debug("BEGIN [%lu]", t_id + 1);
    t_id++;
    participants.clear();
    bcb(t_id);
  });
}

void Client::Get(const std::string &key, get_callback gcb,
    get_timeout_callback gtcb, uint32_t timeout) {

  transport->Timer(0, [this, key, gcb, gtcb, timeout]() {
    Debug("GET [%lu : %s]", t_id, key.c_str());
    // Contact the appropriate shard to get the value.
    int i = part(key, nshards) % ngroups;

    // If needed, add this shard to set of participants and send BEGIN.
    if (participants.find(i) == participants.end()) {
      participants.insert(i);
      bclient[i]->Begin(t_id);
    }

    // Send the GET operation to appropriate shard.
    bclient[i]->Get(key, gcb, gtcb, timeout);
  });
}

void Client::Put(const std::string &key, const std::string &value,
    put_callback pcb, put_timeout_callback ptcb, uint32_t timeout) {
  transport->Timer(0, [this, key, value, pcb, ptcb, timeout]() {
    Debug("PUT [%lu : %s]", t_id, key.c_str());
    // Contact the appropriate shard to set the value.
    int i = part(key, nshards) % ngroups;

    // If needed, add this shard to set of participants and send BEGIN.
    if (participants.find(i) == participants.end()) {
      participants.insert(i);
      bclient[i]->Begin(t_id);
    }

    // Buffering, so no need to wait.
    bclient[i]->Put(key, value, pcb, ptcb, timeout);
  });
}

void Client::Commit(commit_callback ccb, commit_timeout_callback ctcb,
    uint32_t timeout) {
  transport->Timer(0, [this, ccb, ctcb, timeout]() {
    uint64_t reqId = lastReqId++;
    PendingRequest *req = new PendingRequest(reqId);
    pendingReqs[reqId] = req;
    req->ccb = ccb;
    req->ctcb = ctcb;
    req->prepareTimestamp = Timestamp(timeServer.GetTime(), client_id);
    req->callbackInvoked = false;
    req->timeout = timeout;
    
    Prepare(req, timeout);
  });
}

void Client::Prepare(PendingRequest *req, uint32_t timeout) {
  Debug("PREPARE [%lu] at %lu.%lu", t_id, req->prepareTimestamp.getTimestamp(),
      req->prepareTimestamp.getID());
  UW_ASSERT(participants.size() > 0);

  req->outstandingPrepares = 0;
  req->prepareStatus = REPLY_OK;
  req->maxRepliedTs = 0UL;

  for (auto p : participants) {
    bclient[p]->Prepare(req->prepareTimestamp, std::bind(
          &Client::PrepareCallback, this, req->id, std::placeholders::_1,
          std::placeholders::_2), std::bind(&Client::PrepareCallback, this,
            req->id, std::placeholders::_1, std::placeholders::_2), timeout);
    req->outstandingPrepares++;
  }
}

void Client::PrepareCallback(uint64_t reqId, int status, Timestamp ts) {
  Debug("PREPARE [%lu] callback status %d and ts %lu.%lu", t_id, status,
      ts.getTimestamp(), ts.getID());
  auto itr = this->pendingReqs.find(reqId);
  if (itr == this->pendingReqs.end()) {
    Debug("PrepareCallback for terminated request id %lu (txn already committed or aborted.", reqId);
    return;
  }
  PendingRequest *req = itr->second;

  uint64_t proposed = ts.getTimestamp();

  --req->outstandingPrepares;
  switch(status) {
    case REPLY_OK:
      Debug("PREPARE [%lu] OK", t_id);
      break;
    case REPLY_FAIL:
      // abort!
      Debug("PREPARE [%lu] ABORT", t_id);
      req->prepareStatus = REPLY_FAIL;
      req->outstandingPrepares = 0;
      break;
    case REPLY_RETRY:
      req->prepareStatus = REPLY_RETRY;
      if (proposed > req->maxRepliedTs) {
        Debug("PREPARE [%lu] update max reply ts from %lu to %lu.", t_id,
            req->maxRepliedTs, proposed);
        req->maxRepliedTs = proposed;
      }
      break;
    case REPLY_TIMEOUT:
      req->prepareStatus = REPLY_RETRY;
      break;
    case REPLY_ABSTAIN:
      // just ignore abstains
      break;
    default:
      break;
  }

  if (req->outstandingPrepares == 0) {
    HandleAllPreparesReceived(req);
  }
}

void Client::HandleAllPreparesReceived(PendingRequest *req) {
  Debug("All PREPARE's [%lu] received", t_id);
  uint64_t reqId = req->id;
  transaction_status_t abortResult = COMMITTED;
  switch (req->prepareStatus) {
    case REPLY_OK: {
      Debug("COMMIT [%lu]", t_id);
      // application doesn't need to be notified when commit has been acknowledged,
      // so we use empty callback functions and directly call the commit callback
      // function (with commit=true indicating a commit)
      commit_callback ccb = [this, reqId](bool committed) {
        auto itr = this->pendingReqs.find(reqId);
        if (itr != this->pendingReqs.end()) {
          PendingRequest *req = itr->second;
          if (!req->callbackInvoked) {
            req->ccb(COMMITTED);
            req->callbackInvoked = true;
          }
          this->pendingReqs.erase(itr);
          delete req;
        }
      };
      commit_timeout_callback ctcb = [txnId = this->t_id](){
        Warning("COMMIT[%lu] timeout.", txnId);
      };
      for (auto p : participants) {
          bclient[p]->Commit(req->prepareTimestamp.getTimestamp(), ccb, ctcb,
              1000); // we don't really care about the timeout here
      }
      if (!syncCommit) {
        if (!req->callbackInvoked) {
          req->ccb(COMMITTED);
          req->callbackInvoked = true;
        }
      }
      break;
    }
    case REPLY_RETRY: {
      ++req->commitTries;
      if (req->commitTries < COMMIT_RETRIES) {
        statInts["retries"] += 1;
        uint64_t now = timeServer.GetTime();
        if (now > req->maxRepliedTs) {
          req->prepareTimestamp.setTimestamp(now);
        } else {
          // !!! important that the retry timestamp is larger than the max reply
          //   otherwise we will continue retrying with a smaller timestamp for some
          //   small number of rounds when clocks are not tightly synchronized
          req->prepareTimestamp.setTimestamp(req->maxRepliedTs + 1);
        }
        Debug("RETRY [%lu] at [%lu.%lu]", t_id, req->prepareTimestamp.getTimestamp(),
            req->prepareTimestamp.getID());
        Prepare(req, req->timeout); // this timeout should probably be the same as 
        // the timeout passed to Client::Commit, or maybe that timeout / COMMIT_RETRIES
        break;
      } 
      statInts["aborts_max_retries"] += 1;
      abortResult = ABORTED_MAX_RETRIES;
      break;
    }
    case REPLY_FAIL: {
      abortResult = ABORTED_SYSTEM;
      break;
    }
    default: {
      break;
    }
  }
  if (abortResult != COMMITTED) {
    // application doesn't need to be notified when abort has been acknowledged,
    // so we use empty callback functions and directly call the commit callback
    // function (with commit=false indicating an abort)
    abort_callback acb = [this, reqId]() {
      auto itr = this->pendingReqs.find(reqId);
      if (itr != this->pendingReqs.end()) {
        PendingRequest *req = itr->second;
        this->pendingReqs.erase(itr);
        delete req;
      }
    };
    abort_timeout_callback atcb = [txnId = this->t_id](){
      Warning("ABORT[%lu] timeout.", txnId);
    };
    AbortInternal(acb, atcb, ABORT_TIMEOUT); // we don't really care about the timeout here
    if (!req->callbackInvoked) {
      req->ccb(abortResult);
      req->callbackInvoked = true;
    }
  }
}

void Client::Abort(abort_callback acb, abort_timeout_callback atcb,
    uint32_t timeout) {
  // presumably this will be called with empty callbacks as the application can
  // immediately move on to its next transaction without waiting for confirmation
  // that this transaction was aborted
  transport->Timer(0, [this, acb, atcb, timeout]() {
      AbortInternal([](){}, [](){}, timeout);
      acb();
  });
}

void Client::AbortInternal(abort_callback acb, abort_timeout_callback atcb,
    uint32_t timeout) {
  Debug("ABORT [%lu]", t_id);
  for (auto p : participants) {
    bclient[p]->Abort(acb, atcb, timeout);
  }
}

} // namespace tapirstore
