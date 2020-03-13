// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * store/indicusstore/client.cc:
 *   Client to INDICUS transactional storage system.
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

#include "store/indicusstore/client.h"

namespace indicusstore {

using namespace std;

Client::Client(const std::string &configPath, int nShards, int nGroups,
    int closestReplica, Transport *transport, partitioner part, bool syncCommit,
    uint64_t readQuorumSize, bool signedMessages, bool validateProofs,
    const std::string &cryptoConfigPath, TrueTime timeServer) : nshards(nShards),
    ngroups(nGroups), transport(transport), part(part), syncCommit(syncCommit),
    timeServer(timeServer), lastReqId(0UL), config(nullptr) {
  // Initialize all state here;
  client_id = 0;
  while (client_id == 0) {
    random_device rd;
    mt19937_64 gen(rd());
    uniform_int_distribution<uint64_t> dis;
    client_id = dis(gen);
  }
  t_id = (client_id/10000)*10000;

  bclient.reserve(nshards);

  Debug("Initializing Indicus client with id [%lu] %lu", client_id, nshards);

  std::ifstream configStream(configPath);
  if (configStream.fail()) {
    Panic("Unable to read configuration file: %s\n", configPath.c_str());
  }
  config = new transport::Configuration(configStream);

  /* Start a client for each shard. */
  for (uint64_t i = 0; i < ngroups; i++) {
    bclient[i] = new ShardClient(config, transport, client_id, i,
        closestReplica, readQuorumSize, signedMessages, validateProofs,
        cryptoConfigPath, timeServer);
  }

  Debug("Indicus client [%lu] created! %lu %lu", client_id, nshards,
      bclient.size());
}

Client::~Client()
{
    for (auto b : bclient) {
        delete b;
    }
    delete config;
}

/* Begins a transaction. All subsequent operations before a commit() or
 * abort() are part of this transaction.
 */
void Client::Begin() {
  t_id++;
  Debug("BEGIN [%lu]", t_id);
  participants.clear();
}

void Client::Get(const std::string &key, get_callback gcb,
    get_timeout_callback gtcb, uint32_t timeout) {
  Debug("GET [%lu : %s]", t_id, key.c_str());

  // Contact the appropriate shard to get the value.
  int i = part(key, nshards) % ngroups;

  // If needed, add this shard to set of participants and send BEGIN.
  if (participants.find(i) == participants.end()) {
    participants.insert(i);
    bclient[i]->Begin(t_id);
  }

  read_callback rcb = [gcb](int status, const std::string &key,
      const std::string &val, Timestamp ts, const proto::Transaction &dep,
      bool hasDep) {
    gcb(status, key, val, ts);
  };
  read_timeout_callback rtcb = gtcb;

  // Send the GET operation to appropriate shard.
  bclient[i]->Get(t_id, key, rcb, rtcb, timeout);
}

void Client::Put(const std::string &key, const std::string &value,
    put_callback pcb, put_timeout_callback ptcb, uint32_t timeout) {
  Debug("PUT [%lu : %s]", t_id, key.c_str());

  // Contact the appropriate shard to set the value.
  int i = part(key, nshards) % ngroups;

  // If needed, add this shard to set of participants and send BEGIN.
  if (participants.find(i) == participants.end()) {
    participants.insert(i);
    bclient[i]->Begin(t_id);
  }

  // Buffering, so no need to wait.
  bclient[i]->Put(t_id, key, value, pcb, ptcb, timeout);
}

void Client::Commit(commit_callback ccb, commit_timeout_callback ctcb,
    uint32_t timeout) {
  // TODO: this codepath is sketchy and probably has a bug (especially in the
  // failure cases)

  uint64_t reqId = lastReqId++;
  PendingRequest *req = new PendingRequest(reqId);
  pendingReqs[reqId] = req;
  req->ccb = ccb;
  req->ctcb = ctcb;
  req->prepareTimestamp = new Timestamp(timeServer.GetTime(), client_id);
  req->callbackInvoked = false;
  
  Phase1(req, timeout);
}

void Client::Phase1(PendingRequest *req, uint32_t timeout) {
  Debug("PHASE1 [%lu] at %lu", t_id, req->prepareTimestamp->getTimestamp());
  UW_ASSERT(participants.size() > 0);

  for (auto p : participants) {
    bclient[p]->Phase1(t_id, *req->prepareTimestamp, std::bind(
          &Client::Phase1Callback, this, req->id, std::placeholders::_1,
          std::placeholders::_2, std::placeholders::_3),
        std::bind(&Client::Phase1TimeoutCallback, this, req->id,
          std::placeholders::_1, std::placeholders::_2), timeout);
    req->outstandingPhase1s++;
  }
}

void Client::Phase1Callback(uint64_t reqId, proto::CommitDecision decision,
    bool fast, Timestamp ts) {
  Debug("PHASE1 [%lu] callback %d,%lu", t_id, decision, ts.getTimestamp());
  auto itr = this->pendingReqs.find(reqId);
  if (itr == this->pendingReqs.end()) {
    Debug("Phase1Callback for terminated request id %lu (txn already committed or aborted.", reqId);
    return;
  }
  PendingRequest *req = itr->second;

  //uint64_t proposed = ts.getTimestamp();

  req->fast = req->fast && fast;
  --req->outstandingPhase1s;
  switch(decision) {
    case proto::COMMIT:
      Debug("PREPARE [%lu] OK", t_id);
      break;
    case proto::ABORT:
      // abort!
      Debug("PREPARE [%lu] ABORT", t_id);
      req->decision = proto::ABORT;
      req->outstandingPhase1s = 0;
      break;
    /* TODO: are RETRY and ABSTAIN commit decisions?
    case REPLY_RETRY:
      req->prepareStatus = REPLY_RETRY;
      if (proposed > req->maxRepliedTs) {
        req->maxRepliedTs = proposed;
      }
      break;
    case REPLY_TIMEOUT:
      req->prepareStatus = REPLY_RETRY;
      break;
    case REPLY_ABSTAIN:
      // just ignore abstains
      break;*/
    default:
      break;
  }

  if (req->outstandingPhase1s == 0) {
    HandleAllPhase1Received(req);
  }
}

void Client::Phase1TimeoutCallback(uint64_t reqId, int status, Timestamp ts) {
}

void Client::HandleAllPhase1Received(PendingRequest *req) {
  Debug("All PREPARE's [%lu] received", t_id);
  uint64_t reqId = req->id;
  int abortResult = -1;
  switch (req->decision) {
    case proto::COMMIT: {
      Debug("COMMIT [%lu]", t_id);
      // application doesn't need to be notified when commit has been acknowledged,
      // so we use empty callback functions and directly call the commit callback
      // function (with commit=true indicating a commit)
      commit_callback ccb = [this, reqId](bool committed) {
        auto itr = this->pendingReqs.find(reqId);
        if (itr != this->pendingReqs.end()) {
          if (!itr->second->callbackInvoked) {
            itr->second->ccb(RESULT_COMMITTED);
            itr->second->callbackInvoked = true;
          }
          this->pendingReqs.erase(itr);
          delete itr->second;
        }
      };
      commit_timeout_callback ctcb = [](int status){};
      for (auto p : participants) {
          bclient[p]->Commit(t_id, req->prepareTimestamp->getTimestamp(),
              ccb, ctcb, 1000); // we don't really care about the timeout here
      }
      if (!syncCommit) {
        if (!req->callbackInvoked) {
          req->ccb(RESULT_COMMITTED);
          req->callbackInvoked = true;
        }
      }
      break;
    }
    /*case REPLY_RETRY: {
      ++req->commitTries;
      if (req->commitTries < COMMIT_RETRIES) {
        statInts["retries"] += 1;
        uint64_t now = timeServer.GetTime();
        if (now > req->maxRepliedTs) {
          req->prepareTimestamp->setTimestamp(now);
        } else {
          req->prepareTimestamp->setTimestamp(req->maxRepliedTs);
        }
        Debug("RETRY [%lu] at [%lu]", t_id, req->prepareTimestamp->getTimestamp());
        Phase1(req, 1000); // this timeout should probably be the same as 
        // the timeout passed to Client::Commit, or maybe that timeout / COMMIT_RETRIES
        break;
      } 
      statInts["aborts_max_retries"] += 1;
      abortResult = RESULT_MAX_RETRIES;
      break;
    }*/
    case proto::ABORT: {
      abortResult = RESULT_SYSTEM_ABORTED;
      break;
    }
    default: {
      break;
    }
  }
  if (abortResult > 0) {
    // application doesn't need to be notified when abort has been acknowledged,
    // so we use empty callback functions and directly call the commit callback
    // function (with commit=false indicating an abort)
    abort_callback acb = [this, reqId]() {
      auto itr = this->pendingReqs.find(reqId);
      if (itr != this->pendingReqs.end()) {
        this->pendingReqs.erase(itr);
        delete itr->second;
      }
    };
    abort_timeout_callback atcb = [](int status){};
    Abort(acb, atcb, ABORT_TIMEOUT); // we don't really care about the timeout here
    if (!req->callbackInvoked) {
      req->ccb(abortResult);
      req->callbackInvoked = true;
    }
  }
}

void Client::Phase2(PendingRequest *req, uint32_t timeout) {
  Debug("PHASE2 [%lu] at %lu", t_id, req->prepareTimestamp->getTimestamp());

  for (auto p : participants) {
    // replicas need to check that there are P1 replies for each shard in the
    //    read set and write set
    bclient[p]->Phase2(t_id, std::bind(&Client::Phase2Callback, this, req->id,
          std::placeholders::_1),
        std::bind(&Client::Phase2TimeoutCallback, this, req->id,
          std::placeholders::_1), timeout);
    req->outstandingPhase2s++;
  }

}

void Client::Phase2Callback(uint64_t reqId, proto::CommitDecision decision) {
}

void Client::Phase2TimeoutCallback(uint64_t reqId, int status) {
}

void Client::Abort(abort_callback acb, abort_timeout_callback atcb,
    uint32_t timeout) {
  // presumably this will be called with empty callbacks as the application can
  // immediately move on to its next transaction without waiting for confirmation
  // that this transaction was aborted
  Debug("ABORT [%lu]", t_id);

  for (auto p : participants) {
    bclient[p]->Abort(t_id, acb, atcb, timeout);
  }
}

/* Return statistics of most recent transaction. */
vector<int>
Client::Stats()
{
    vector<int> v;
    return v;
}

} // namespace indicusstore
