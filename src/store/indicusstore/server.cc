// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * store/indicusstore/server.cc:
 *   Implementation of a single transactional key-value server.
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

#include "store/indicusstore/server.h"

#include "lib/tcptransport.h"

namespace indicusstore {

Server::Server(const transport::Configuration &config, int groupIdx, int idx,
    Transport *transport) : config(config),
    groupIdx(groupIdx), idx(idx), transport(transport), occType(TAPIR) {
  transport->Register(this, config, groupIdx, idx);
}

Server::~Server() {
}

void Server::ReceiveMessage(const TransportAddress &remote,
      const std::string &type, const std::string &data, void *meta_data) {
  proto::Read read;
  proto::Prepare1 prepare1;
  proto::Commit commit;
  proto::Abort abort;

  if (type == read.GetTypeName()) {
    read.ParseFromString(data);
    HandleRead(remote, read);
  } else if (type == prepare1.GetTypeName()) {
    prepare1.ParseFromString(data);
    HandlePrepare1(remote, prepare1);
  } else if (type == commit.GetTypeName()) {
    commit.ParseFromString(data);
    HandleCommit(remote, commit);
  } else if (type == abort.GetTypeName()) {
    abort.ParseFromString(data);
    HandleAbort(remote, abort);
  } else {
    Panic("Received unexpected message type: %s", type.c_str());
  }
}


void Server::Load(const string &key, const string &value,
    const Timestamp timestamp) {
}

void Server::HandleRead(const TransportAddress &remote,
    const proto::Read &msg) {
  std::pair<Timestamp, std::string> tsVal;
  bool exists = store.get(msg.key(), tsVal);

  proto::ReadReply reply;
  reply.set_req_id(msg.req_id());
  reply.set_key(msg.key());
  if (exists) {
    reply.set_status(REPLY_OK);
    reply.set_value(tsVal.second);
    tsVal.first.serialize(reply.mutable_timestamp());
  } else {
    reply.set_status(REPLY_FAIL);
  }

  transport->SendMessage(this, remote, reply);
}

void Server::HandlePrepare1(const TransportAddress &remote,
    const proto::Prepare1 &msg) {
  Timestamp retryTs;
  int32_t status = DoOCCCheck(msg.txn_id(), msg.txn(), msg.timestamp(), retryTs);
}

void Server::HandleCommit(const TransportAddress &remote,
    const proto::Commit &msg) {
}

void Server::HandleAbort(const TransportAddress &remote,
    const proto::Abort &msg) {
}

int32_t Server::DoOCCCheck(uint64_t id, const Transaction &txn,
    const Timestamp &proposedTs, Timestamp &retryTs) {
  switch (occType) {
    case TAPIR:
      return DoTAPIROCCCheck(id, txn, proposedTs, retryTs);
    case MVTSO:
      return DoMVTSOOCCCheck(id, txn, proposedTs);
    default:
      Panic("Unknown OCC type: %d.", occType);
      return REPLY_FAIL;
  }
}

int32_t Server::DoTAPIROCCCheck(uint64_t id, const Transaction &txn,
    const Timestamp &proposedTs, Timestamp &retryTs) {
  Debug("[%lu] START PREPARE", id);

  Debug("[%lu] Active transactions: %lu.", id, active.size());
  active.erase(id);

  if (prepared.find(id) != prepared.end()) {
    if (prepared[id].first == proposedTs) {
      Warning("[%lu] Already Prepared!", id);
      return REPLY_OK;
    } else {
      // run the checks again for a new timestamp
      prepared.erase(id);
    }
  }

  // do OCC checks
  std::unordered_map<string, std::set<Timestamp>> pWrites;
  GetPreparedWrites(pWrites);
  std::unordered_map<string, std::set<Timestamp>> pReads;
  GetPreparedReads(pReads);

  // check for conflicts with the read set
  for (auto &read : txn.getReadSet()) {
    std::pair<Timestamp, Timestamp> range;
    bool ret = store.getRange(read.first, read.second, range);

    Debug("Range %lu %lu %lu", read.second.getTimestamp(),
        range.first.getTimestamp(), range.second.getTimestamp());

    // if we don't have this key then no conflicts for read
    if (!ret) {
      continue;
    }

    // if we don't have this version then no conflicts for read
    if (range.first != read.second) {
      continue;
    }

    // if the value is still valid
    if (!range.second.isValid()) {
      // check pending writes.
      if (pWrites.find(read.first) != pWrites.end()) {
        Debug("[%lu] ABSTAIN rw conflict w/ prepared key:%s", id,
            read.first.c_str());
        stats.Increment("abstains", 1);
        return REPLY_ABSTAIN;
      }
    } else {
      // if value is not still valid, then abort.
      if (proposedTs <= range.first) {
        Warning("timestamp %lu <= range.first %lu (range.second %lu)",
            proposedTs.getTimestamp(), range.first.getTimestamp(),
            range.second.getTimestamp());
      }
      //UW_ASSERT(timestamp > range.first);
      Debug("[%lu] ABORT rw conflict: %lu > %lu", id, proposedTs.getTimestamp(),
          range.second.getTimestamp());
      stats.Increment("aborts", 1);
      return REPLY_FAIL;
    }
  }

  // check for conflicts with the write set
  for (auto &write : txn.getWriteSet()) {
    std::pair<Timestamp, std::string> val;
    // if this key is in the store
    if (store.get(write.first, val)) {
      Timestamp lastRead;
      bool ret;

      // if the last committed write is bigger than the timestamp,
      // then can't accept
      if (val.first > proposedTs) {
        Debug("[%lu] RETRY ww conflict w/ prepared key:%s", id,
            write.first.c_str());
        retryTs = val.first;
        stats.Increment("retries_committed_write", 1);
        return REPLY_RETRY;
      }

      // if last committed read is bigger than the timestamp, can't
      // accept this transaction, but can propose a retry timestamp

      // we get the timestamp of the last read ever on this object
      ret = store.getLastRead(write.first, lastRead);

      // if this key is in the store and has been read before
      if (ret && lastRead > proposedTs) {
        Debug("[%lu] RETRY wr conflict w/ prepared key:%s", id,
            write.first.c_str());
        retryTs = lastRead;
        return REPLY_RETRY;
      }
    }

    // if there is a pending write for this key, greater than the
    // proposed timestamp, retry
    if (pWrites.find(write.first) != pWrites.end()) {
      std::set<Timestamp>::iterator it =
          pWrites[write.first].upper_bound(proposedTs);
      if ( it != pWrites[write.first].end() ) {
        Debug("[%lu] RETRY ww conflict w/ prepared key:%s", id,
            write.first.c_str());
        retryTs = *it;
        stats.Increment("retries_prepared_write", 1);
        return REPLY_RETRY;
      }
    }


    //if there is a pending read for this key, greater than the
    //propsed timestamp, abstain
    if (pReads.find(write.first) != pReads.end() &&
        pReads[write.first].upper_bound(proposedTs) !=
        pReads[write.first].end()) {
      Debug("[%lu] ABSTAIN wr conflict w/ prepared key:%s",
            id, write.first.c_str());
      stats.Increment("abstains", 1);
      return REPLY_ABSTAIN;
    }
  }

  // Otherwise, prepare this transaction for commit
  prepared[id] = std::make_pair(proposedTs, txn);
  Debug("[%lu] PREPARED TO COMMIT", id);

  return REPLY_OK;
}

int32_t Server::DoMVTSOOCCCheck(uint64_t id, const Transaction &txn,
    const Timestamp &ts) {
  Panic("Not implemented.");
  return REPLY_FAIL;
}

void Server::GetPreparedWrites(
    std::unordered_map<std::string, std::set<Timestamp>> &writes) {
  // gather up the set of all writes that are currently prepared
  for (const auto &t : prepared) {
    for (const auto &write : t.second.second.getWriteSet()) {
      writes[write.first].insert(t.second.first);
    }
  }
}

void Server::GetPreparedReads(
    std::unordered_map<std::string, std::set<Timestamp>> &reads) {
  // gather up the set of all writes that are currently prepared
  for (const auto &t : prepared) {
    for (const auto &read : t.second.second.getReadSet()) {
      reads[read.first].insert(t.second.first);
    }
  }
}

} // namespace indicusstore
