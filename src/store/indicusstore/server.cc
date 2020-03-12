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
    Transport *transport, TrueTime timeServer) : config(config),
    groupIdx(groupIdx), idx(idx), id(groupIdx * config.n + idx),
    transport(transport), occType(TAPIR),
    signedMessages(false), validateProofs(false), cryptoConfig(nullptr),
    timeDelta(100UL), timeServer(timeServer) {
  privateKey = cryptoConfig->getClientPrivateKey(id);
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
    if (ValidateSignedMessage(signedMessage, cryptoConfig)) {
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
  std::pair<Timestamp, Server::Value> tsVal;
  bool exists;
  store.get(msg.key(), Timestamp(msg.timestamp()), tsVal);

  proto::ReadReply reply;
  reply.set_req_id(msg.req_id());
  reply.set_key(msg.key());
  if (exists) {
    reply.set_status(REPLY_OK);
    reply.set_committed_value(tsVal.second.val);
    tsVal.first.serialize(reply.mutable_committed_timestamp());
    if (validateProofs) {
      proto::WriteProof committedProof;
      *committedProof.mutable_txn() = tsVal.second.txn;
      if (signedMessages) {
        for (const auto &reply : tsVal.second.signedPhase2Replies) {
          proto::SignedMessage *signedP2Reply = committedProof.mutable_signed_p2_replies()->add_msgs();   
          *signedP2Reply = reply;
        }
      } else {
        for (const auto &reply : tsVal.second.phase2Replies) {
          proto::Phase2Reply *p2Reply = committedProof.mutable_p2_replies()->add_replies();   
          *p2Reply = reply;
        }
      }
      *reply.mutable_committed_proof() = committedProof;
    }
  } else {
    reply.set_status(REPLY_FAIL);
  }

  if (occType == MVTSO) {
    /* update rts */
    Timestamp localTs(timeServer.GetTime());
    // add delta to current local time
    localTs.setTimestamp(localTs.getTimestamp() + timeDelta);
    if (Timestamp(msg.timestamp()) <= localTs) {
      rts[msg.key()][msg.txn_id()].insert(msg.timestamp());
    }

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
      for (const auto &w : mostRecent.writeset()) {
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
        SignMessage(preparedWrite, privateKey, id, signedMessage);
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
    SignMessage(reply, privateKey, id, signedMessage);
    transport->SendMessage(this, remote, signedMessage);
  } else {
    transport->SendMessage(this, remote, reply);
  }
}

void Server::HandlePhase1(const TransportAddress &remote,
    const proto::Phase1 &msg) {
  Timestamp retryTs;
  proto::Transaction txnConflict;
  proto::Phase1Reply::ConcurrencyControlResult result = DoOCCCheck(msg.txn_id(),
      msg.txn(), msg.txn().timestamp(), retryTs);
  p1Decisions[msg.txn_id()] = result;

  proto::Phase1Reply reply;
  reply.set_req_id(msg.req_id());
  reply.set_status(REPLY_OK);
  reply.set_ccr(result);
  if (result == proto::Phase1Reply::RETRY) {
    retryTs.serialize(reply.mutable_retry_timestamp());
  }

  if (validateProofs) {
    *reply.mutable_txn_conflict() = txnConflict;
  }

  if (signedMessages) {
    proto::SignedMessage signedMessage;
    signedMessage.set_msg(reply.SerializeAsString());
    signedMessage.set_type(reply.GetTypeName());
    signedMessage.set_process_id(id);
    SignMessage(reply, privateKey, id, signedMessage);
    transport->SendMessage(this, remote, signedMessage);
  } else {
    transport->SendMessage(this, remote, reply);
  }
}

void Server::HandlePhase2(const TransportAddress &remote,
      const proto::Phase2 &msg) {
  std::vector<proto::Phase1Reply> prepare1Replies;
  bool validated = true;
  if (signedMessages) {
    proto::Phase1Reply prepare1Reply;
    for (const auto &signedPhase1Reply : msg.signed_p1_replies().msgs()) {
      if (ValidateSignedMessage(signedPhase1Reply, cryptoConfig)) {
        prepare1Reply.ParseFromString(signedPhase1Reply.msg());
        prepare1Replies.push_back(prepare1Reply);
      } else {
        validated = false;
        break;
      }
    }
  } else {
    for (const auto &prepare1Reply : msg.p1_replies().replies()) {
      prepare1Replies.push_back(prepare1Reply);
    }
  }

  proto::CommitDecision decision;
  if (validated) {
    decision = IndicusDecide(prepare1Replies, &config);
  } else {
    decision = proto::CommitDecision::ABORT;
  }

  proto::Phase2Reply reply;
  reply.set_decision(decision); 
  p2Decisions[msg.txn_id()] = decision;

  if (signedMessages) {
    proto::SignedMessage signedMessage;
    signedMessage.set_msg(reply.SerializeAsString());
    signedMessage.set_type(reply.GetTypeName());
    signedMessage.set_process_id(id);
    SignMessage(reply, privateKey, id, signedMessage);
    transport->SendMessage(this, remote, signedMessage);
  } else {
    transport->SendMessage(this, remote, reply);
  }
}


void Server::HandleWriteback(const TransportAddress &remote,
    const proto::Writeback &msg) {
  Commit(msg.txn_id(), msg.txn(), Timestamp(msg.txn().timestamp()));
}

void Server::HandleAbort(const TransportAddress &remote,
    const proto::Abort &msg) {
  uint64_t txnId = 0UL;
  prepared.erase(txnId);
  aborted.insert(txnId);
}

proto::Phase1Reply::ConcurrencyControlResult Server::DoOCCCheck(uint64_t id, const proto::Transaction &txn,
    const Timestamp &proposedTs, Timestamp &retryTs) {
  switch (occType) {
    case TAPIR:
      return DoTAPIROCCCheck(id, txn, proposedTs, retryTs);
    case MVTSO:
      return DoMVTSOOCCCheck(id, txn, proposedTs);
    default:
      Panic("Unknown OCC type: %d.", occType);
      return proto::Phase1Reply::ABORT;
  }
}

proto::Phase1Reply::ConcurrencyControlResult Server::DoTAPIROCCCheck(uint64_t id, const proto::Transaction &txn,
    const Timestamp &proposedTs, Timestamp &retryTs) {
  Debug("[%lu] START PREPARE", id);

  Debug("[%lu] Active transactions: %lu.", id, active.size());
  active.erase(id);

  if (prepared.find(id) != prepared.end()) {
    if (prepared[id].first == proposedTs) {
      Warning("[%lu] Already Prepared!", id);
      return proto::Phase1Reply::COMMIT;
    } else {
      // run the checks again for a new timestamp
      prepared.erase(id);
    }
  }

  // do OCC checks
  std::unordered_map<string, std::set<Timestamp>> pWrites;
  GetPreparedWriteTimestamps(pWrites);
  std::unordered_map<string, std::set<Timestamp>> pReads;
  GetPreparedReadTimestamps(pReads);

  // check for conflicts with the read set
  for (const auto &read : txn.readset()) {
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
        Debug("[%lu] ABSTAIN rw conflict w/ prepared key:%s", id,
            read.key().c_str());
        stats.Increment("abstains", 1);
        return proto::Phase1Reply::ABSTAIN;
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
      return proto::Phase1Reply::ABORT;
    }
  }

  // check for conflicts with the write set
  for (const auto &write : txn.writeset()) {
    std::pair<Timestamp, Server::Value> val;
    // if this key is in the store
    if (store.get(write.key(), val)) {
      Timestamp lastRead;
      bool ret;

      // if the last committed write is bigger than the timestamp,
      // then can't accept
      if (val.first > proposedTs) {
        Debug("[%lu] RETRY ww conflict w/ prepared key:%s", id,
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
      if (ret && lastRead > proposedTs) {
        Debug("[%lu] RETRY wr conflict w/ prepared key:%s", id,
            write.key().c_str());
        retryTs = lastRead;
        return proto::Phase1Reply::RETRY;
      }
    }

    // if there is a pending write for this key, greater than the
    // proposed timestamp, retry
    if (pWrites.find(write.key()) != pWrites.end()) {
      std::set<Timestamp>::iterator it =
          pWrites[write.key()].upper_bound(proposedTs);
      if ( it != pWrites[write.key()].end() ) {
        Debug("[%lu] RETRY ww conflict w/ prepared key:%s", id,
            write.key().c_str());
        retryTs = *it;
        stats.Increment("retries_prepared_write", 1);
        return proto::Phase1Reply::RETRY;
      }
    }

    //if there is a pending read for this key, greater than the
    //propsed timestamp, abstain
    if (pReads.find(write.key()) != pReads.end() &&
        pReads[write.key()].upper_bound(proposedTs) !=
        pReads[write.key()].end()) {
      Debug("[%lu] ABSTAIN wr conflict w/ prepared key:%s",
            id, write.key().c_str());
      stats.Increment("abstains", 1);
      return proto::Phase1Reply::ABSTAIN;
    }
  }

  // Otherwise, prepare this transaction for commit
  prepared[id] = std::make_pair(proposedTs, txn);
  Debug("[%lu] PREPARED TO COMMIT", id);

  return proto::Phase1Reply::COMMIT;
}

proto::Phase1Reply::ConcurrencyControlResult Server::DoMVTSOOCCCheck(
    uint64_t id, const proto::Transaction &txn, const Timestamp &ts) {
  Timestamp localTs(timeServer.GetTime());
  // add delta to current local time
  localTs.setTimestamp(localTs.getTimestamp() + timeDelta);
  if (ts > localTs) {
    return proto::Phase1Reply::ABORT;
  }

  std::unordered_map<std::string, std::set<Timestamp>> preparedWrites;
  GetPreparedWriteTimestamps(preparedWrites);
  for (const auto &read : txn.readset()) {
    std::set<Timestamp> committedWrites;
    GetCommittedWrites(read.key(), read.readtime(), committedWrites);
    for (const auto &committedTs : committedWrites) {
      // readVersion < committedTs < ts
      //     GetCommittedWrites only returns writes larger than readVersion
      if (committedTs < ts) {
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
  for (const auto &write : txn.writeset()) {
    std::set<Timestamp> committedReads;
    for (const auto &committedTs : committedReads) {
      if (ts < committedTs) {
        return proto::Phase1Reply::ABORT;
      }
    }

    const auto preparedReadsItr = preparedReads.find(write.key());
    if (preparedReadsItr != preparedReads.end()) {
      for (const auto &preparedReadTxn : preparedReadsItr->second) {
        bool isDep = false;
        for (const auto &dep : preparedReadTxn.deps()) {
          if (txn.id() == dep.id()) {
            isDep = true;
            break;
          }
        }

        bool isReadVersionEarlier = false;
        for (const auto &read : preparedReadTxn.readset()) {
          if (read.key() == write.key()) {
            isReadVersionEarlier = Timestamp(read.readtime()) < ts;
            break;
          }
        }
        if (!isDep && isReadVersionEarlier &&
            ts < Timestamp(preparedReadTxn.timestamp())) {
          return proto::Phase1Reply::ABSTAIN;
        }
      }
    }

    auto rtsItr = rts.find(write.key());
    if (rtsItr != rts.end()) {
      auto rtsTxnItr = rtsItr->second.find(txn.id());
      if (rtsTxnItr != rtsItr->second.end()) {
        for (const auto &readTs : rtsTxnItr->second) {
          if (readTs > ts) {
            return proto::Phase1Reply::ABSTAIN;
          }
        }
      }
    }
    // TODO: add additional rts dep check to shrink abort window
  }

  prepared[id] = std::make_pair(ts, txn);
  bool allFinished = true;
  for (const auto &dep : txn.deps()) {
    if (committed.find(dep.id()) == committed.end() &&
        aborted.find(dep.id()) == aborted.end()) {
      allFinished = false;
      break;
    }
  }

  if (!allFinished) {
    return proto::Phase1Reply::WAIT;
  } else {
    for (const auto &dep : txn.deps()) {
      if (committed.find(dep.id()) != committed.end()) {
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
    for (const auto &write : t.second.second.writeset()) {
      writes[write.key()].insert(t.second.first);
    }
  }
}

void Server::GetPreparedWrites(
      std::unordered_map<std::string, std::vector<proto::Transaction>> &writes) {
  for (const auto &t : prepared) {
    for (const auto &write : t.second.second.writeset()) {
      writes[write.key()].push_back(t.second.second);
    }
  }
}



void Server::GetPreparedReadTimestamps(
    std::unordered_map<std::string, std::set<Timestamp>> &reads) {
  // gather up the set of all writes that are currently prepared
  for (const auto &t : prepared) {
    for (const auto &read : t.second.second.readset()) {
      reads[read.key()].insert(t.second.first);
    }
  }
}

void Server::GetPreparedReads(
    std::unordered_map<std::string, std::vector<proto::Transaction>> &reads) {
  // gather up the set of all writes that are currently prepared
  for (const auto &t : prepared) {
    for (const auto &read : t.second.second.readset()) {
      reads[read.key()].push_back(t.second.second);
    }
  }
}


void Server::GetCommittedWrites(const std::string &key, const Timestamp &ts,
    std::set<Timestamp> &writes) {
  std::vector<std::pair<Timestamp, Server::Value>> values;
  if (store.getCommittedAfter(key, ts, values)) {
    for (const auto &p : values) {
      writes.insert(p.first);
    }
  }
}

void Server::GetCommittedReads(const std::string &key, const Timestamp &ts,
    std::set<Timestamp> &reads) {
  /*std::vector<std::pair<Timestamp, Server::Value>> values;
  if (store.getCommittedAfter(key, ts, values)) {
    for (const auto &p : values) {
      writes.insert(p.first);
    }
  }

  for (const auto &t : prepared) {
    for (const auto &read : t.second.second.readset()) {
      reads[read.key()].insert(t.second.first);
    }
  }*/
}


void Server::Commit(uint64_t txnId, const proto::Transaction &txn,
    const Timestamp &timestamp) {
  for (const auto &read : txn.readset()) {
    store.commitGet(read.key(), read.readtime(), timestamp);
  }
  
  Value val;
  for (const auto &write : txn.writeset()) {
    val.val = write.value();
    store.put(write.key(), val, timestamp);
  }

  prepared.erase(txnId);
  committed.insert(txnId);
}

} // namespace indicusstore
