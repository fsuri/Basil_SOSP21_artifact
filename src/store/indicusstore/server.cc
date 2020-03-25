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
#include "store/indicusstore/common.h"

namespace indicusstore {

Server::Server(const transport::Configuration &config, int groupIdx, int idx,
    Transport *transport, KeyManager *keyManager, bool signedMessages,
    bool validateProofs, uint64_t timeDelta, OCCType occType, TrueTime timeServer) :
    config(config), groupIdx(groupIdx), idx(idx), id(groupIdx * config.n + idx),
    transport(transport), occType(occType),
    signedMessages(signedMessages), validateProofs(validateProofs), keyManager(keyManager),
    timeDelta(timeDelta), timeServer(timeServer) {
  transport->Register(this, config, groupIdx, idx);
}

Server::~Server() {
}

void Server::ReceiveMessage(const TransportAddress &remote,
      const std::string &t, const std::string &d, void *meta_data) {
  proto::SignedMessage signedMessage;
  proto::Read read;
  proto::Phase1 phase1;
  proto::Phase2 phase2;
  proto::Writeback writeback;
  proto::Abort abort;

  std::string type;
  std::string data;
  if (t == signedMessage.GetTypeName()) {
    signedMessage.ParseFromString(d);
    if (ValidateSignedMessage(signedMessage, keyManager)) {
      type = signedMessage.type();
      data = signedMessage.msg();
    } else {
      return;
    }
  } else {
    type = t;
    data = d;
  }

  if (type == read.GetTypeName()) {
    read.ParseFromString(data);
    HandleRead(remote, read);
  } else if (type == phase1.GetTypeName()) {
    phase1.ParseFromString(data);
    HandlePhase1(remote, phase1);
  } else if (type == phase2.GetTypeName()) {
    phase2.ParseFromString(data);
    HandlePhase2(remote, phase2);
  } else if (type == writeback.GetTypeName()) {
    writeback.ParseFromString(data);
    HandleWriteback(remote, writeback);
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
  Debug("READ[%lu,%lu] for key %s.", msg.txn_id(), msg.req_id(), msg.key().c_str());
  Timestamp ts(msg.timestamp());
  if (CheckHighWatermark(ts)) {
    // ignore request if beyond high watermark
    return;
  }

  std::pair<Timestamp, Server::Value> tsVal;
  bool exists = store.get(msg.key(), ts, tsVal);

  proto::ReadReply reply;
  reply.set_req_id(msg.req_id());
  reply.set_key(msg.key());
  if (exists) {
    reply.set_status(REPLY_OK);
    reply.set_committed_value(tsVal.second.val);
    tsVal.first.serialize(reply.mutable_committed_timestamp());
    if (validateProofs) {
      *reply.mutable_committed_proof() = tsVal.second.proof;
    }
  } else {
    reply.set_status(REPLY_FAIL);
  }

  if (occType == MVTSO) {
    /* update rts */
    rts[msg.key()][msg.txn_id()].insert(ts);

    /* add prepared deps */
    std::unordered_map<std::string, std::vector<proto::Transaction>> writes;
    GetPreparedWrites(writes);
    auto itr = writes.find(msg.key());
    if (itr != writes.end()) {
      // there is a prepared write for the key being read
      proto::PreparedWrite preparedWrite;
      proto::Transaction mostRecent;
      for (const auto &t : itr->second) {
        if (Timestamp(t.timestamp()) > Timestamp(mostRecent.timestamp())) {
          mostRecent = t;
        }
      }

      std::string preparedValue;
      for (const auto &w : mostRecent.write_set()) {
        if (w.key() == msg.key()) {
          preparedValue = w.value();
          break;
        }
      }

      preparedWrite.set_value(preparedValue);
      *preparedWrite.mutable_timestamp() = mostRecent.timestamp();
      *preparedWrite.mutable_txn() = mostRecent;

      if (signedMessages) {
        proto::SignedMessage signedMessage;
        signedMessage.set_msg(preparedWrite.SerializeAsString());
        signedMessage.set_type(preparedWrite.GetTypeName());
        signedMessage.set_process_id(id);
        SignMessage(preparedWrite, keyManager->GetPrivateKey(id), id, signedMessage);
        *reply.mutable_signed_prepared() = signedMessage;
      } else {
        *reply.mutable_prepared() = preparedWrite;
      }
      reply.set_status(REPLY_OK);
    }
  }

  if (signedMessages) {
    proto::SignedMessage signedMessage;
    signedMessage.set_msg(reply.SerializeAsString());
    signedMessage.set_type(reply.GetTypeName());
    signedMessage.set_process_id(id);
    SignMessage(reply, keyManager->GetPrivateKey(id), id, signedMessage);
    transport->SendMessage(this, remote, signedMessage);
  } else {
    transport->SendMessage(this, remote, reply);
  }
}

void Server::HandlePhase1(const TransportAddress &remote,
    const proto::Phase1 &msg) {
  Debug("PHASE1[%lu,%lu] with ts %lu.", msg.txn_digest(), msg.req_id(),
      msg.txn().timestamp().timestamp());

  Timestamp retryTs;
  proto::CommittedProof conflict;
  proto::Phase1Reply::ConcurrencyControlResult result = DoOCCCheck(
      msg.txn_digest(), msg.txn(), retryTs, conflict);
  p1Decisions[msg.txn_digest()] = result;

  proto::Phase1Reply reply;
  reply.set_req_id(msg.req_id());
  reply.set_status(REPLY_OK);
  reply.set_ccr(result);
  /* TODO: are retries a thing?
  if (result == proto::Phase1Reply::RETRY) {
    retryTs.serialize(reply.mutable_retry_timestamp());
  }*/

  if (validateProofs) {
    *reply.mutable_committed_conflict() = conflict;
  }

  if (signedMessages) {
    proto::SignedMessage signedMessage;
    signedMessage.set_msg(reply.SerializeAsString());
    signedMessage.set_type(reply.GetTypeName());
    signedMessage.set_process_id(id);
    SignMessage(reply, keyManager->GetPrivateKey(id), id, signedMessage);
    transport->SendMessage(this, remote, signedMessage);
  } else {
    transport->SendMessage(this, remote, reply);
  }
}

void Server::HandlePhase2(const TransportAddress &remote,
      const proto::Phase2 &msg) {
  Debug("PHASE1[%lu,%lu].", msg.txn_digest(), msg.req_id());

  std::map<int, std::vector<proto::Phase1Reply>> groupedPhase1Replies;
  bool validated = true;
  if (signedMessages) {
    proto::Phase1Reply phase1Reply;
    for (const auto &group : msg.signed_p1_replies().replies()) {
      std::vector<proto::Phase1Reply> phase1Replies;
      for (const auto &signedPhase1Reply : group.second.msgs()) {
        if (ValidateSignedMessage(signedPhase1Reply, keyManager)) {
          phase1Reply.ParseFromString(signedPhase1Reply.msg());
          phase1Replies.push_back(phase1Reply);
        } else {
          validated = false;
          break;
        }
      }
      groupedPhase1Replies.insert(std::make_pair(group.first, phase1Replies));
    }
  } else {
    for (const auto &group : msg.p1_replies().replies()) {
      std::vector<proto::Phase1Reply> phase1Replies;
      for (const auto &phase1Reply : group.second.replies()) {
        phase1Replies.push_back(phase1Reply);
      }
      groupedPhase1Replies.insert(std::make_pair(group.first, phase1Replies));
    }
  }

  proto::CommitDecision decision;
  if (validated) {
    decision = IndicusDecide(groupedPhase1Replies, &config, validateProofs);
  } else {
    decision = proto::CommitDecision::ABORT;
  }

  proto::Phase2Reply reply;
  reply.set_req_id(msg.req_id());
  reply.set_decision(decision); 
  p2Decisions[msg.txn_digest()] = decision;

  if (signedMessages) {
    proto::SignedMessage signedMessage;
    signedMessage.set_msg(reply.SerializeAsString());
    signedMessage.set_type(reply.GetTypeName());
    signedMessage.set_process_id(id);
    SignMessage(reply, keyManager->GetPrivateKey(id), id, signedMessage);
    transport->SendMessage(this, remote, signedMessage);
  } else {
    transport->SendMessage(this, remote, reply);
  }
}

void Server::HandleWriteback(const TransportAddress &remote,
    const proto::Writeback &msg) {
  Debug("WRITEBACK[%lu,%lu]  with decision %d.", msg.txn_digest(), msg.req_id(),
      msg.decision());
  if (msg.decision() == proto::COMMIT) {
    Commit(msg.txn_digest(), msg.txn());
  } else {
    Abort(msg.txn_digest());
  }
}

void Server::HandleAbort(const TransportAddress &remote,
    const proto::Abort &msg) {
  Abort(msg.txn_id());
}

proto::Phase1Reply::ConcurrencyControlResult Server::DoOCCCheck(uint64_t txnDigest,
    const proto::Transaction &txn, Timestamp &retryTs,
    proto::CommittedProof &conflict) {
  switch (occType) {
    case TAPIR:
      return DoTAPIROCCCheck(txnDigest, txn, retryTs);
    case MVTSO:
      return DoMVTSOOCCCheck(txnDigest, txn, conflict);
    default:
      Panic("Unknown OCC type: %d.", occType);
      return proto::Phase1Reply::ABORT;
  }
}

proto::Phase1Reply::ConcurrencyControlResult Server::DoTAPIROCCCheck(
    uint64_t txnDigest, const proto::Transaction &txn, Timestamp &retryTs) {
  Debug("[%lu] START PREPARE", txnDigest);

  Debug("[%lu] Active transactions: %lu.", txnDigest, active.size());
  active.erase(txnDigest);

  if (prepared.find(txnDigest) != prepared.end()) {
    if (prepared[txnDigest].first == txn.timestamp()) {
      Warning("[%lu] Already Prepared!", txnDigest);
      return proto::Phase1Reply::COMMIT;
    } else {
      // run the checks again for a new timestamp
      prepared.erase(txnDigest);
    }
  }

  // do OCC checks
  std::unordered_map<string, std::set<Timestamp>> pWrites;
  GetPreparedWriteTimestamps(pWrites);
  std::unordered_map<string, std::set<Timestamp>> pReads;
  GetPreparedReadTimestamps(pReads);

  // check for conflicts with the read set
  for (const auto &read : txn.read_set()) {
    std::pair<Timestamp, Timestamp> range;
    bool ret = store.getRange(read.key(), read.readtime(), range);

    Debug("Range %lu %lu %lu", Timestamp(read.readtime()).getTimestamp(),
        range.first.getTimestamp(), range.second.getTimestamp());

    // if we don't have this key then no conflicts for read
    if (!ret) {
      continue;
    }

    // if we don't have this version then no conflicts for read
    if (range.first != read.readtime()) {
      continue;
    }

    // if the value is still valid
    if (!range.second.isValid()) {
      // check pending writes.
      if (pWrites.find(read.key()) != pWrites.end()) {
        Debug("[%lu] ABSTAIN rw conflict w/ prepared key:%s", txnDigest,
            read.key().c_str());
        stats.Increment("abstains", 1);
        return proto::Phase1Reply::ABSTAIN;
      }
    } else {
      // if value is not still valtxnDigest, then abort.
      if (Timestamp(txn.timestamp()) <= range.first) {
        Warning("timestamp %lu <= range.first %lu (range.second %lu)",
            txn.timestamp().timestamp(), range.first.getTimestamp(),
            range.second.getTimestamp());
      }
      //UW_ASSERT(timestamp > range.first);
      Debug("[%lu] ABORT rw conflict: %lu > %lu", txnDigest,
          txn.timestamp().timestamp(), range.second.getTimestamp());
      stats.Increment("aborts", 1);
      return proto::Phase1Reply::ABORT;
    }
  }

  // check for conflicts with the write set
  for (const auto &write : txn.write_set()) {
    std::pair<Timestamp, Server::Value> val;
    // if this key is in the store
    if (store.get(write.key(), val)) {
      Timestamp lastRead;
      bool ret;

      // if the last committed write is bigger than the timestamp,
      // then can't accept
      if (val.first > Timestamp(txn.timestamp())) {
        Debug("[%lu] RETRY ww conflict w/ prepared key:%s", txnDigest,
            write.key().c_str());
        retryTs = val.first;
        stats.Increment("retries_committed_write", 1);
        return proto::Phase1Reply::RETRY;
      }

      // if last committed read is bigger than the timestamp, can't
      // accept this transaction, but can propose a retry timestamp

      // we get the timestamp of the last read ever on this object
      ret = store.getLastRead(write.key(), lastRead);

      // if this key is in the store and has been read before
      if (ret && lastRead > Timestamp(txn.timestamp())) {
        Debug("[%lu] RETRY wr conflict w/ prepared key:%s", txnDigest,
            write.key().c_str());
        retryTs = lastRead;
        return proto::Phase1Reply::RETRY;
      }
    }

    // if there is a pending write for this key, greater than the
    // proposed timestamp, retry
    if (pWrites.find(write.key()) != pWrites.end()) {
      std::set<Timestamp>::iterator it =
          pWrites[write.key()].upper_bound(txn.timestamp());
      if ( it != pWrites[write.key()].end() ) {
        Debug("[%lu] RETRY ww conflict w/ prepared key:%s", txnDigest,
            write.key().c_str());
        retryTs = *it;
        stats.Increment("retries_prepared_write", 1);
        return proto::Phase1Reply::RETRY;
      }
    }

    //if there is a pending read for this key, greater than the
    //propsed timestamp, abstain
    if (pReads.find(write.key()) != pReads.end() &&
        pReads[write.key()].upper_bound(txn.timestamp()) !=
        pReads[write.key()].end()) {
      Debug("[%lu] ABSTAIN wr conflict w/ prepared key:%s",
            txnDigest, write.key().c_str());
      stats.Increment("abstains", 1);
      return proto::Phase1Reply::ABSTAIN;
    }
  }

  // Otherwise, prepare this transaction for commit
  Prepare(txnDigest, txn);

  Debug("[%lu] PREPARED TO COMMIT", txnDigest);

  return proto::Phase1Reply::COMMIT;
}

proto::Phase1Reply::ConcurrencyControlResult Server::DoMVTSOOCCCheck(
    uint64_t txnDigest, const proto::Transaction &txn,
    proto::CommittedProof &conflict) {
  Timestamp ts(txn.timestamp());
  if (CheckHighWatermark(ts)) {
    return proto::Phase1Reply::ABORT;
  }

  std::unordered_map<std::string, std::set<Timestamp>> preparedWrites;
  GetPreparedWriteTimestamps(preparedWrites);
  for (const auto &read : txn.read_set()) {
    std::vector<std::pair<Timestamp, Server::Value>> committedWrites;
    GetCommittedWrites(read.key(), read.readtime(), committedWrites);
    for (const auto &committedWrite : committedWrites) {
      // readVersion < committedTs < ts
      //     GetCommittedWrites only returns writes larger than readVersion
      if (committedWrite.first < ts) {
        if (validateProofs) {
          conflict = committedWrite.second.proof;
        } 
        return proto::Phase1Reply::ABORT; // TODO: return conflicting txn
      }
    }

    const auto preparedWritesItr = preparedWrites.find(read.key());
    if (preparedWritesItr != preparedWrites.end()) {
      for (const auto &preparedTs : preparedWritesItr->second) {
        if (Timestamp(read.readtime()) < preparedTs && preparedTs < ts) {
          return proto::Phase1Reply::ABSTAIN; // TODO: return conflicting txn
        }
      }
    }
  }
  
  std::unordered_map<std::string, std::vector<proto::Transaction>> preparedReads;
  GetPreparedReads(preparedReads);
  for (const auto &write : txn.write_set()) {
    std::set<std::pair<Timestamp, Timestamp>> committedReads;
    GetCommittedReads(write.key(), committedReads);

    if (committedReads.size() > 0) {
      for (const auto &committedReadTs : committedReads) {
        if (committedReadTs.second < ts && ts < committedReadTs.first) {
          return proto::Phase1Reply::RETRY;
        }
      }
    }

    const auto preparedReadsItr = preparedReads.find(write.key());
    if (preparedReadsItr != preparedReads.end()) {
      for (const auto &preparedReadTxn : preparedReadsItr->second) {
        bool isDep = false;
        for (const auto &dep : preparedReadTxn.deps()) {
          uint64_t depDigest = TransactionDigest(dep);
          if (txnDigest == depDigest) {
            isDep = true;
            break;
          }
        }

        bool isReadVersionEarlier = false;
        for (const auto &read : preparedReadTxn.read_set()) {
          if (read.key() == write.key()) {
            isReadVersionEarlier = Timestamp(read.readtime()) < ts;
            break;
          }
        }
        if (!isDep && isReadVersionEarlier &&
            ts < Timestamp(preparedReadTxn.timestamp())) {
          return proto::Phase1Reply::RETRY;
        }
      }
    }

    auto rtsItr = rts.find(write.key());
    if (rtsItr != rts.end()) {
      auto rtsTxnItr = rtsItr->second.find(txnDigest);
      if (rtsTxnItr != rtsItr->second.end()) {
        for (const auto &readTs : rtsTxnItr->second) {
          if (readTs > ts) {
            return proto::Phase1Reply::RETRY;
          }
        }
      }
    }
    // TODO: add additional rts dep check to shrink abort window
  }

  Prepare(txnDigest, txn);

  bool allFinished = true;
  for (const auto &dep : txn.deps()) {
    uint64_t depDigest = TransactionDigest(dep);
    if (committed.find(depDigest) == committed.end() &&
        aborted.find(depDigest) == aborted.end()) {
      allFinished = false;
      depends[depDigest].insert(txnDigest);
    }
  }

  if (!allFinished) {
    return proto::Phase1Reply::WAIT;
  } else {
    for (const auto &dep : txn.deps()) {
      uint64_t depDigest = TransactionDigest(dep);
      if (committed.find(depDigest) != committed.end()) {
        if (Timestamp(dep.timestamp()) > ts) {
          return proto::Phase1Reply::ABORT;
        }
      } else {
        return proto::Phase1Reply::ABORT;
      }
    } 
    return proto::Phase1Reply::COMMIT;
  }
}

void Server::GetPreparedWriteTimestamps(
    std::unordered_map<std::string, std::set<Timestamp>> &writes) {
  // gather up the set of all writes that are currently prepared
  for (const auto &t : prepared) {
    for (const auto &write : t.second.second.write_set()) {
      writes[write.key()].insert(t.second.first);
    }
  }
}

void Server::GetPreparedWrites(
      std::unordered_map<std::string, std::vector<proto::Transaction>> &writes) {
  for (const auto &t : prepared) {
    for (const auto &write : t.second.second.write_set()) {
      writes[write.key()].push_back(t.second.second);
    }
  }
}



void Server::GetPreparedReadTimestamps(
    std::unordered_map<std::string, std::set<Timestamp>> &reads) {
  // gather up the set of all writes that are currently prepared
  for (const auto &t : prepared) {
    for (const auto &read : t.second.second.read_set()) {
      reads[read.key()].insert(t.second.first);
    }
  }
}

void Server::GetPreparedReads(
    std::unordered_map<std::string, std::vector<proto::Transaction>> &reads) {
  // gather up the set of all writes that are currently prepared
  for (const auto &t : prepared) {
    for (const auto &read : t.second.second.read_set()) {
      reads[read.key()].push_back(t.second.second);
    }
  }
}

void Server::Prepare(uint64_t txnDigest, const proto::Transaction &txn) {
  prepared[txnDigest] = std::make_pair(Timestamp(txn.timestamp()), txn);
}

void Server::GetCommittedWrites(const std::string &key, const Timestamp &ts,
    std::vector<std::pair<Timestamp, Server::Value>> &writes) {
  std::vector<std::pair<Timestamp, Server::Value>> values;
  if (store.getCommittedAfter(key, ts, values)) {
    for (const auto &p : values) {
      writes.push_back(p);
    }
  }
}

void Server::GetCommittedReads(const std::string &key,
      std::set<std::pair<Timestamp, Timestamp>> &reads) {
  auto itr = committedReads.find(key);
  if (itr != committedReads.end()) {
    reads = itr->second;
  }
}

void Server::Commit(uint64_t txnDigest, const proto::Transaction &txn) {
  Timestamp ts(txn.timestamp());
  for (const auto &read : txn.read_set()) {
    store.commitGet(read.key(), read.readtime(), ts);
    committedReads[read.key()].insert(std::make_pair(ts, read.readtime()));
  }
  
  Value val;
  for (const auto &write : txn.write_set()) {
    val.val = write.value();
    store.put(write.key(), val, ts);
  }

  prepared.erase(txnDigest);
  committed.insert(txnDigest);
  CheckDependents(txnDigest);
}

void Server::Abort(uint64_t txnId) {
  prepared.erase(txnId);
  aborted.insert(txnId);
  CheckDependents(txnId);
}

void Server::CheckDependents(uint64_t txnId) {
}

bool Server::CheckHighWatermark(const Timestamp &ts) {
  Timestamp highWatermark(timeServer.GetTime());
  // add delta to current local time
  highWatermark.setTimestamp(highWatermark.getTimestamp() + timeDelta);
  return ts > highWatermark;
}

} // namespace indicusstore
