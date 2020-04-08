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

#include <bitset>

#include "lib/tcptransport.h"
#include "store/indicusstore/common.h"

namespace indicusstore {

Server::Server(const transport::Configuration &config, int groupIdx, int idx,
    int numShards, int numGroups, Transport *transport, KeyManager *keyManager,
    bool signedMessages,
    bool validateProofs, uint64_t timeDelta, OCCType occType, partitioner part,
    TrueTime timeServer) :
    config(config), groupIdx(groupIdx), idx(idx), numShards(numShards),
    numGroups(numGroups), id(groupIdx * config.n + idx),
    transport(transport), occType(occType), part(part),
    signedMessages(signedMessages), validateProofs(validateProofs), keyManager(keyManager),
    timeDelta(timeDelta), timeServer(timeServer) {
  transport->Register(this, config, groupIdx, idx);
}

Server::~Server() {
}

void Server::ReceiveMessage(const TransportAddress &remote,
      const std::string &t, const std::string &d, void *meta_data) {
  proto::SignedMessage signedMessage;
  proto::PackedMessage packedMessage;
  proto::Read read;
  proto::Phase1 phase1;
  proto::Phase2 phase2;
  proto::Writeback writeback;
  proto::Abort abort;

  std::string type;
  std::string data;
  if (t == signedMessage.GetTypeName()) {
    if (!signedMessage.ParseFromString(d)) {
      return;
    }

    if (!ValidateSignedMessage(signedMessage, keyManager, data, type)) {
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
  Value val;
  val.val = value;
  val.proof.mutable_txn()->set_client_id(0);
  val.proof.mutable_txn()->set_client_seq_num(0);
  val.proof.mutable_txn()->mutable_timestamp()->set_timestamp(0);
  val.proof.mutable_txn()->mutable_timestamp()->set_id(0);
  store.put(key, val, timestamp);
  if (key.length() == 5 && key[0] == 0) {
    std::cerr << std::bitset<8>(key[0]) << ' '
              << std::bitset<8>(key[1]) << ' '
              << std::bitset<8>(key[2]) << ' '
              << std::bitset<8>(key[3]) << ' '
              << std::bitset<8>(key[4]) << ' '
              << std::endl;
  }
}

void Server::HandleRead(const TransportAddress &remote,
    const proto::Read &msg) {
  Debug("READ[%lu] for key %s.", msg.req_id(), BytesToHex(msg.key(), 16).c_str());
  Timestamp ts(msg.timestamp());
  /*if (CheckHighWatermark(ts)) {
    // ignore request if beyond high watermark
    return;
  }*/

  std::pair<Timestamp, Server::Value> tsVal;
  bool exists = store.get(msg.key(), ts, tsVal);

  proto::ReadReply reply;
  reply.set_req_id(msg.req_id());
  reply.set_key(msg.key());
  if (exists) {
    Debug("READ[%lu] Committed value of length %lu bytes with ts %lu.%lu.", msg.req_id(),
        tsVal.second.val.length(), tsVal.first.getTimestamp(), tsVal.first.getID());
    reply.set_status(REPLY_OK);
    reply.mutable_committed()->set_value(tsVal.second.val);
    tsVal.first.serialize(reply.mutable_committed()->mutable_timestamp());
    if (validateProofs) {
      *reply.mutable_committed()->mutable_proof() = tsVal.second.proof;
    }
  } else {
    reply.set_status(REPLY_FAIL);
  }

  if (occType == MVTSO) {
    /* update rts */
    // TODO: how to track RTS by transaction without knowing transaction digest?
    rts[msg.key()].insert(ts);

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
      *preparedWrite.mutable_txn_digest() = TransactionDigest(mostRecent);

      if (signedMessages) {
        proto::SignedMessage signedMessage;
        SignMessage(preparedWrite, keyManager->GetPrivateKey(id), id, signedMessage);
        *reply.mutable_signed_prepared() = signedMessage;
      } else {
        *reply.mutable_prepared() = preparedWrite;
      }
      reply.set_status(REPLY_OK);
    }
  }

  // Only need to sign prepared portion of reply
  transport->SendMessage(this, remote, reply);
}

void Server::HandlePhase1(const TransportAddress &remote,
    const proto::Phase1 &msg) {
  Debug("PHASE1[%lu:%lu:%lu] with ts %lu.",
      msg.txn().client_id(),
      msg.txn().client_seq_num(),
      msg.req_id(), msg.txn().timestamp().timestamp());

  if (validateProofs) {
    for (const auto &dep : msg.txn().deps()) {
      if (!ValidateDependency(dep, &config, signedMessages, keyManager)) {
        // safe to ignore Byzantine client
        return;
      }
    }
  }
  
  ongoing[msg.txn_digest()] = msg.txn();

  Timestamp retryTs;
  proto::CommittedProof conflict;
  proto::Phase1Reply::ConcurrencyControlResult result = DoOCCCheck(
      msg.req_id(), remote,
      msg.txn_digest(), msg.txn(), retryTs, conflict);
  p1Decisions[msg.txn_digest()] = result;

  if (result != proto::Phase1Reply::WAIT) {
    SendPhase1Reply(msg.req_id(), result, conflict, remote);
  }
}

void Server::HandlePhase2(const TransportAddress &remote,
      const proto::Phase2 &msg) {
  Debug("PHASE2[%lu].", msg.req_id());

  proto::CommitDecision decision;
  if (validateProofs) {
    std::map<int, std::vector<proto::Phase1Reply>> groupedPhase1Replies;
    bool repliesValid = true;
    if (signedMessages) {
      proto::Phase1Reply phase1Reply;
      for (const auto &group : msg.signed_p1_replies().replies()) {
        std::vector<proto::Phase1Reply> phase1Replies;
        for (const auto &signedPhase1Reply : group.second.msgs()) {
          if (ValidateSignedMessage(signedPhase1Reply, keyManager, phase1Reply)) {
            phase1Replies.push_back(phase1Reply);
          } else {
            repliesValid = false;
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
    
    if (repliesValid) {
      decision = IndicusDecide(groupedPhase1Replies, &config, validateProofs,
          signedMessages, keyManager);
      if (decision != msg.decision()) {
        // ignore Byzantine clients
        return;
      }
    } else {
      // ignore Byzantine clients
      return;
    }
  } else {
    decision = msg.decision();
  }

  proto::Phase2Reply reply;
  reply.set_req_id(msg.req_id());
  reply.set_decision(decision); 
  *reply.mutable_txn_digest() = msg.txn_digest();

  p2Decisions[msg.txn_digest()] = decision;

  if (signedMessages) {
    proto::SignedMessage signedMessage;
    SignMessage(reply, keyManager->GetPrivateKey(id), id, signedMessage);
    transport->SendMessage(this, remote, signedMessage);
  } else {
    transport->SendMessage(this, remote, reply);
  }
}

void Server::HandleWriteback(const TransportAddress &remote,
    const proto::Writeback &msg) {
  Debug("WRITEBACK[%lu]  with decision %d.", msg.req_id(), msg.decision());

  if (validateProofs) {
    if (!ValidateProof(msg.proof(), &config, signedMessages, keyManager)) {
      // ignore Writeback without valid proof
      return;
    }
  }

  if (msg.decision() == proto::COMMIT) {
    Commit(msg.txn_digest(), msg.txn());
  } else {
    Abort(msg.txn_digest());
  }
}

void Server::HandleAbort(const TransportAddress &remote,
    const proto::Abort &msg) {
  Abort(msg.txn_digest());
}

proto::Phase1Reply::ConcurrencyControlResult Server::DoOCCCheck(
    uint64_t reqId, const TransportAddress &remote,
    const std::string &txnDigest, const proto::Transaction &txn,
    Timestamp &retryTs, proto::CommittedProof &conflict) {
  switch (occType) {
    case TAPIR:
      return DoTAPIROCCCheck(txnDigest, txn, retryTs);
    case MVTSO:
      return DoMVTSOOCCCheck(reqId, remote, txnDigest, txn, conflict);
    default:
      Panic("Unknown OCC type: %d.", occType);
      return proto::Phase1Reply::ABORT;
  }
}

proto::Phase1Reply::ConcurrencyControlResult Server::DoTAPIROCCCheck(
    const std::string &txnDigest, const proto::Transaction &txn,
    Timestamp &retryTs) {
  Debug("[%s] START PREPARE", txnDigest.c_str());

  Debug("[%s] Active transactions: %lu.", txnDigest.c_str(), active.size());
  active.erase(txnDigest);

  if (prepared.find(txnDigest) != prepared.end()) {
    if (prepared[txnDigest].first == txn.timestamp()) {
      Warning("[%s] Already Prepared!", txnDigest.c_str());
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
        Debug("[%lu,%lu] ABSTAIN rw conflict w/ prepared key %s.",
            txn.client_id(),
            txn.client_seq_num(),
            BytesToHex(read.key(), 16).c_str());
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
      Debug("[%s] ABORT rw conflict: %lu > %lu", txnDigest.c_str(),
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
        Debug("[%s] RETRY ww conflict w/ prepared key:%s", txnDigest.c_str(),
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
        Debug("[%s] RETRY wr conflict w/ prepared key:%s", txnDigest.c_str(),
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
        Debug("[%s] RETRY ww conflict w/ prepared key:%s", txnDigest.c_str(),
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
      Debug("[%s] ABSTAIN wr conflict w/ prepared key: %s",
            txnDigest.c_str(), write.key().c_str());
      stats.Increment("abstains", 1);
      return proto::Phase1Reply::ABSTAIN;
    }
  }

  // Otherwise, prepare this transaction for commit
  Prepare(txnDigest, txn);

  Debug("[%s] PREPARED TO COMMIT", txnDigest.c_str());

  return proto::Phase1Reply::COMMIT;
}

proto::Phase1Reply::ConcurrencyControlResult Server::DoMVTSOOCCCheck(
    uint64_t reqId, const TransportAddress &remote,
    const std::string &txnDigest, const proto::Transaction &txn,
    proto::CommittedProof &conflict) {
  Timestamp ts(txn.timestamp());
  if (CheckHighWatermark(ts)) {
    Debug("[%s] ABORT ts %lu beyond high watermark.", txnDigest.c_str(),
        ts.getTimestamp());
    return proto::Phase1Reply::ABSTAIN;
  }

  std::unordered_map<std::string, std::set<Timestamp>> preparedWrites;
  GetPreparedWriteTimestamps(preparedWrites);
  for (const auto &read : txn.read_set()) {
    // TODO: remove this check when txns only contain read set/write set for the
    //   shards stored at this replica
    if (!IsKeyOwned(read.key())) {
      continue;
    }

    std::vector<std::pair<Timestamp, Server::Value>> committedWrites;
    GetCommittedWrites(read.key(), read.readtime(), committedWrites);
    for (const auto &committedWrite : committedWrites) {
      // readVersion < committedTs < ts
      //     GetCommittedWrites only returns writes larger than readVersion
      if (committedWrite.first < ts) {
        if (validateProofs) {
          conflict = committedWrite.second.proof;
        } 
        Debug("[%lu,%lu] ABORT wr conflict committed write for key %s:"
            " this txn's read ts %lu.%lu < committed ts %lu.%lu < this txn's ts %lu.%lu.",
            txn.client_id(),
            txn.client_seq_num(),
            BytesToHex(read.key(), 16).c_str(),
            read.readtime().timestamp(),
            read.readtime().id(), committedWrite.first.getTimestamp(),
            committedWrite.first.getID(), ts.getTimestamp(), ts.getID());
        return proto::Phase1Reply::ABORT; // TODO: return conflicting txn
      }
    }

    const auto preparedWritesItr = preparedWrites.find(read.key());
    if (preparedWritesItr != preparedWrites.end()) {
      for (const auto &preparedTs : preparedWritesItr->second) {
        if (Timestamp(read.readtime()) < preparedTs && preparedTs < ts) {
          Debug("[%lu,%lu] ABSTAIN wr conflict prepared write for key %s:"
            " this txn's read ts %lu.%lu < prepared ts %lu.%lu < this txn's ts %lu.%lu.",
              txn.client_id(),
              txn.client_seq_num(),
              BytesToHex(read.key(), 16).c_str(),
              read.readtime().timestamp(),
              read.readtime().id(), preparedTs.getTimestamp(),
              preparedTs.getID(), ts.getTimestamp(), ts.getID());
          return proto::Phase1Reply::ABSTAIN; // TODO: return conflicting txn
        }
      }
    }
  }
  
  std::unordered_map<std::string, std::vector<proto::Transaction>> preparedReads;
  GetPreparedReads(preparedReads);
  for (const auto &write : txn.write_set()) {
    if (!IsKeyOwned(write.key())) {
      continue;
    }

    std::set<std::pair<Timestamp, Timestamp>> committedReads;
    GetCommittedReads(write.key(), committedReads);

    if (committedReads.size() > 0) {
      for (const auto &committedReadTs : committedReads) {
        if (committedReadTs.second < ts && ts < committedReadTs.first) {
          Debug("[%lu,%lu] ABORT rw conflict committed read for key %s: committed"
              " read ts %lu.%lu < this txn's ts %lu.%lu < committed ts %lu.%lu.",
              txn.client_id(),
              txn.client_seq_num(),
              BytesToHex(write.key(), 16).c_str(),
              committedReadTs.second.getTimestamp(),
              committedReadTs.second.getID(), ts.getTimestamp(),
              ts.getID(), committedReadTs.first.getTimestamp(), committedReadTs.first.getID());
          return proto::Phase1Reply::ABORT;
        }
      }
    }

    const auto preparedReadsItr = preparedReads.find(write.key());
    if (preparedReadsItr != preparedReads.end()) {
      for (const auto &preparedReadTxn : preparedReadsItr->second) {
        bool isDep = false;
        for (const auto &dep : preparedReadTxn.deps()) {
          if (txnDigest == dep.prepared().txn_digest()) {
            isDep = true;
            break;
          }
        }

        bool isReadVersionEarlier = false;
        Timestamp readTs;
        for (const auto &read : preparedReadTxn.read_set()) {
          if (read.key() == write.key()) {
            readTs = Timestamp(read.readtime());
            isReadVersionEarlier = readTs < ts;
            break;
          }
        }
        if (!isDep && isReadVersionEarlier &&
            ts < Timestamp(preparedReadTxn.timestamp())) {
          Debug("[%lu,%lu] ABSTAIN rw conflict prepared read for key %s: prepared"
              " read ts %lu.%lu < this txn's ts %lu.%lu < committed ts %lu.%lu.",
              txn.client_id(),
              txn.client_seq_num(),
              BytesToHex(write.key(), 16).c_str(),
              readTs.getTimestamp(),
              readTs.getID(), ts.getTimestamp(),
              ts.getID(), preparedReadTxn.timestamp().timestamp(),
              preparedReadTxn.timestamp().id());
          return proto::Phase1Reply::ABSTAIN;
        }
      }
    }

    auto rtsItr = rts.find(write.key());
    if (rtsItr != rts.end()) {
      auto rtsLB = rtsItr->second.lower_bound(ts);
      if (rtsLB != rtsItr->second.end() && *rtsLB > ts) {
        Debug("[%lu,%lu] ABSTAIN larger rts acquired for key %s: rts %lu.%lu >"
            " this txn's ts %lu.%lu.",
            txn.client_id(),
            txn.client_seq_num(),
            BytesToHex(write.key(), 16).c_str(),
            rtsLB->getTimestamp(),
            rtsLB->getID(), ts.getTimestamp(), ts.getID());
        return proto::Phase1Reply::ABSTAIN;
      }
    }
    // TODO: add additional rts dep check to shrink abort window
    //    Is this still a thing?
  }

  Prepare(txnDigest, txn);

  bool allFinished = true;
  for (const auto &dep : txn.deps()) {
    if (committed.find(dep.prepared().txn_digest()) == committed.end() &&
        aborted.find(dep.prepared().txn_digest()) == aborted.end()) {
      allFinished = false;
      dependents[dep.prepared().txn_digest()].insert(txnDigest);
      auto dependenciesItr = waitingDependencies.find(txnDigest);
      if (dependenciesItr == waitingDependencies.end()) {
        auto inserted =waitingDependencies.insert(std::make_pair(txnDigest,
              WaitingDependency()));
        UW_ASSERT(inserted.second);
        dependenciesItr = inserted.first;
      }
      dependenciesItr->second.reqId = reqId;
      dependenciesItr->second.remote = &remote;
      dependenciesItr->second.deps.insert(dep.prepared().txn_digest());
    }
  }

  if (!allFinished) {
    return proto::Phase1Reply::WAIT;
  } else {
    return CheckDependencies(txn);
  }
}

void Server::GetPreparedWriteTimestamps(
    std::unordered_map<std::string, std::set<Timestamp>> &writes) {
  // gather up the set of all writes that are currently prepared
  for (const auto &t : prepared) {
    for (const auto &write : t.second.second.write_set()) {
      if (IsKeyOwned(write.key())) {
        writes[write.key()].insert(t.second.first);
      }
    }
  }
}

void Server::GetPreparedWrites(
      std::unordered_map<std::string, std::vector<proto::Transaction>> &writes) {
  for (const auto &t : prepared) {
    for (const auto &write : t.second.second.write_set()) {
      if (IsKeyOwned(write.key())) {
        writes[write.key()].push_back(t.second.second);
      }
    }
  }
}

void Server::GetPreparedReadTimestamps(
    std::unordered_map<std::string, std::set<Timestamp>> &reads) {
  // gather up the set of all writes that are currently prepared
  for (const auto &t : prepared) {
    for (const auto &read : t.second.second.read_set()) {
      if (IsKeyOwned(read.key())) {
        reads[read.key()].insert(t.second.first);
      }
    }
  }
}

void Server::GetPreparedReads(
    std::unordered_map<std::string, std::vector<proto::Transaction>> &reads) {
  // gather up the set of all writes that are currently prepared
  for (const auto &t : prepared) {
    for (const auto &read : t.second.second.read_set()) {
      if (IsKeyOwned(read.key())) {
        reads[read.key()].push_back(t.second.second);
      }
    }
  }
}

void Server::Prepare(const std::string &txnDigest,
    const proto::Transaction &txn) {
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

void Server::Commit(const std::string &txnDigest,
    const proto::Transaction &txn) {
  Timestamp ts(txn.timestamp());
  for (const auto &read : txn.read_set()) {
    if (!IsKeyOwned(read.key())) {
      continue;
    }
    store.commitGet(read.key(), read.readtime(), ts);
    committedReads[read.key()].insert(std::make_pair(ts, read.readtime()));
  }
  
  Value val;
  if (validateProofs) {
  }

  for (const auto &write : txn.write_set()) {
    if (!IsKeyOwned(write.key())) {
      continue;
    }

    Debug("COMMIT[%lu,%lu] Committing write for key %s.",
        txn.client_id(), txn.client_seq_num(), BytesToHex(write.key(), 16).c_str());
    val.val = write.value();
    store.put(write.key(), val, ts);

    auto rtsItr = rts.find(write.key());
    if (rtsItr != rts.end()) {
      auto itr = rtsItr->second.begin();
      auto endItr = rtsItr->second.upper_bound(ts);
      while (itr != endItr) {
        itr = rtsItr->second.erase(itr);
      }
    }
  }


  ongoing.erase(txnDigest);
  prepared.erase(txnDigest);
  committed.insert(txnDigest);
  CheckDependents(txnDigest);
}

void Server::Abort(const std::string &txnDigest) {
  ongoing.erase(txnDigest);
  prepared.erase(txnDigest);
  aborted.insert(txnDigest);
  CheckDependents(txnDigest);
}

void Server::CheckDependents(const std::string &txnDigest) {
  auto dependentsItr = dependents.find(txnDigest);
  if (dependentsItr != dependents.end()) {
    for (const auto &dependent : dependentsItr->second) {
      auto dependenciesItr = waitingDependencies.find(dependent);
      UW_ASSERT(dependenciesItr != waitingDependencies.end());

      dependenciesItr->second.deps.erase(txnDigest);
      if (dependenciesItr->second.deps.size() == 0) {
        proto::Phase1Reply::ConcurrencyControlResult result = CheckDependencies(
            dependent);
        waitingDependencies.erase(dependent);
        proto::CommittedProof conflict;
        SendPhase1Reply(dependenciesItr->second.reqId, result, conflict,
            *dependenciesItr->second.remote);
      }
    }
  }
}

proto::Phase1Reply::ConcurrencyControlResult Server::CheckDependencies(
    const std::string &txnDigest) {
  auto txnItr = ongoing.find(txnDigest);
  UW_ASSERT(txnItr != ongoing.end());
  return CheckDependencies(txnItr->second);
}

proto::Phase1Reply::ConcurrencyControlResult Server::CheckDependencies(
    const proto::Transaction &txn) {
  for (const auto &dep : txn.deps()) {
    if (committed.find(dep.prepared().txn_digest()) != committed.end()) {
      if (Timestamp(dep.prepared().timestamp()) > Timestamp(txn.timestamp())) {
        return proto::Phase1Reply::ABORT;
      }
    } else {
      return proto::Phase1Reply::ABORT;
    }
  } 
  return proto::Phase1Reply::COMMIT;
}

bool Server::CheckHighWatermark(const Timestamp &ts) {
  Timestamp highWatermark(timeServer.GetTime());
  // add delta to current local time
  highWatermark.setTimestamp(highWatermark.getTimestamp() + timeDelta);
  Debug("High watermark: %lu.", highWatermark.getTimestamp());
  return ts > highWatermark;
}

void Server::SendPhase1Reply(uint64_t reqId,
    proto::Phase1Reply::ConcurrencyControlResult result,
    const proto::CommittedProof &conflict, const TransportAddress &remote) {
  proto::Phase1Reply reply;
  reply.set_req_id(reqId);
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
    SignMessage(reply, keyManager->GetPrivateKey(id), id, signedMessage);
    transport->SendMessage(this, remote, signedMessage);
  } else {
    transport->SendMessage(this, remote, reply);
  }

}

} // namespace indicusstore
