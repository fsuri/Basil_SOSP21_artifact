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

#include "lib/assert.h"
#include "lib/tcptransport.h"
#include "store/indicusstore/common.h"
#include "store/indicusstore/phase1validator.h"

namespace indicusstore {

Server::Server(const transport::Configuration &config, int groupIdx, int idx,
    int numShards, int numGroups, Transport *transport, KeyManager *keyManager,
    bool signedMessages, bool validateProofs, bool hashDigest,
    uint64_t timeDelta, OCCType occType, partitioner part, uint64_t readDepSize,
    TrueTime timeServer) :
    config(config), groupIdx(groupIdx), idx(idx), numShards(numShards),
    numGroups(numGroups), id(groupIdx * config.n + idx),
    transport(transport), occType(occType), part(part), readDepSize(readDepSize),
    signedMessages(signedMessages), validateProofs(validateProofs),
    hashDigest(hashDigest), keyManager(keyManager), timeDelta(timeDelta),
    timeServer(timeServer) {
  transport->Register(this, config, groupIdx, idx);
  _Latency_Init(&committedReadInsertLat, "committed_read_insert_lat");
  
  // this is needed purely from loading data without executing transactions
  proto::CommittedProof proof;
  proof.mutable_txn()->set_client_id(0);
  proof.mutable_txn()->set_client_seq_num(0);
  proof.mutable_txn()->mutable_timestamp()->set_timestamp(0);
  proof.mutable_txn()->mutable_timestamp()->set_id(0);
  committed.insert(std::make_pair("", proof));
}

Server::~Server() {
}

void Server::ReceiveMessage(const TransportAddress &remote,
      const std::string &type, const std::string &data, void *meta_data) {
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
  auto committedItr = committed.find("");
  UW_ASSERT(committedItr != committed.end());
  val.proof = &committedItr->second;
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
  Debug("READ[%lu:%lu] for key %s with ts %lu.%lu.", msg.timestamp().id(),
      msg.req_id(), BytesToHex(msg.key(), 16).c_str(),
      msg.timestamp().timestamp(), msg.timestamp().id());
  Timestamp ts(msg.timestamp());
  if (CheckHighWatermark(ts)) {
    // ignore request if beyond high watermark
    Debug("Read timestamp beyond high watermark.");
    return;
  }

  std::pair<Timestamp, Server::Value> tsVal;
  bool exists = store.get(msg.key(), ts, tsVal);

  readReply.Clear();
  readReply.set_req_id(msg.req_id());
  readReply.set_key(msg.key());
  if (exists) {
    Debug("READ[%lu] Committed value of length %lu bytes with ts %lu.%lu.",
        msg.req_id(), tsVal.second.val.length(), tsVal.first.getTimestamp(),
        tsVal.first.getID());
    readReply.mutable_committed()->set_value(tsVal.second.val);
    tsVal.first.serialize(readReply.mutable_committed()->mutable_timestamp());
    if (validateProofs) {
      *readReply.mutable_committed()->mutable_proof() = *tsVal.second.proof;
    }
  }

  if (occType == MVTSO) {
    /* update rts */
    // TODO: how to track RTS by transaction without knowing transaction digest?
    rts[msg.key()].insert(ts);

    /* add prepared deps */
    auto itr = preparedWrites.find(msg.key());
    if (itr != preparedWrites.end() && itr->second.size() > 0) {
      // there is a prepared write for the key being read
      mostRecent.Clear();
      for (const auto &t : itr->second) {
        if (t.first > Timestamp(mostRecent.timestamp())) {
          mostRecent = *t.second;
        }
      }

      std::string preparedValue;
      for (const auto &w : mostRecent.write_set()) {
        if (w.key() == msg.key()) {
          preparedValue = w.value();
          break;
        }
      }

      Debug("Prepared write with most recent ts %lu.%lu.", mostRecent.timestamp().timestamp(),
          mostRecent.timestamp().id());

      // TODO: currently limit depth of deps to 1 (no chains of dependencies)
      //   TODO: temporarily undo this TODO ^
      // if (mostRecent.deps_size() == 0) {
        preparedWrite.Clear();
        preparedWrite.set_value(preparedValue);
        *preparedWrite.mutable_timestamp() = mostRecent.timestamp();
        *preparedWrite.mutable_txn_digest() = TransactionDigest(mostRecent, hashDigest);

        if (signedMessages) {
          signedMessage.Clear();
          SignMessage(preparedWrite, keyManager->GetPrivateKey(id), id,
              readReply.mutable_signed_prepared());
        } else {
          *readReply.mutable_prepared() = preparedWrite;
        }
      //}
    }
  }

  // Only need to sign prepared portion of readReply
  transport->SendMessage(this, remote, readReply);
}

void Server::HandlePhase1(const TransportAddress &remote,
    const proto::Phase1 &msg) {
  std::string txnDigest = TransactionDigest(msg.txn(), hashDigest);
  Debug("PHASE1[%lu:%lu][%s] with ts %lu.", msg.txn().client_id(),
      msg.txn().client_seq_num(), BytesToHex(txnDigest, 16).c_str(),
      msg.txn().timestamp().timestamp());

  if (validateProofs && signedMessages) {
    for (const auto &dep : msg.txn().deps()) {
      if (!ValidateDependency(dep, &config, readDepSize, keyManager)) {
        Debug("VALIDATE Dependency failed for txn %s.",
            BytesToHex(txnDigest, 16).c_str());
        // safe to ignore Byzantine client
        return;
      }
    }
  }
  
  ongoing[txnDigest] = msg.txn();

  Timestamp retryTs;
  proto::ConcurrencyControl::Result result = DoOCCCheck(msg.req_id(),
      remote, txnDigest, msg.txn(), retryTs, conflict);

  // p1Decisions[txnDigest] = result;

  if (result != proto::ConcurrencyControl::WAIT) {
    SendPhase1Reply(msg.req_id(), result, conflict, txnDigest, remote);
  }
}

void Server::HandlePhase2(const TransportAddress &remote,
      const proto::Phase2 &msg) {
  const proto::Transaction *txn;
  std::string computedTxnDigest;
  const std::string *txnDigest = &computedTxnDigest;
  if (validateProofs) {
    if (!msg.has_txn() && !msg.has_txn_digest()) {
      Debug("PHASE2 message contains neither txn nor txn_digest.");
      return;
    } 

    if (msg.has_txn_digest()) {
      auto txnItr = ongoing.find(msg.txn_digest());
      if (txnItr == ongoing.end()) {
        Debug("PHASE2[%s] message does not contain txn, but have not seen"
            " txn_digest previously.", BytesToHex(msg.txn_digest(), 16).c_str());
        return;
      }

      txn = &txnItr->second;
      txnDigest = &msg.txn_digest();
    } else {
      txn = &msg.txn();
      computedTxnDigest = TransactionDigest(msg.txn(), hashDigest);
      txnDigest = &computedTxnDigest;
    }
  }

  Debug("PHASE2[%s].", BytesToHex(*txnDigest, 16).c_str());

  if (validateProofs && signedMessages && !ValidateP1Replies(msg.decision(),
        false, txn, txnDigest, msg.grouped_sigs(), keyManager, &config)) {
    Debug("VALIDATE P1Replies failed.");
    return;
  }

  phase2Reply.Clear();
  phase2Reply.set_req_id(msg.req_id());
  phase2Reply.mutable_p2_decision()->set_decision(msg.decision()); 
  if (validateProofs) {
    *phase2Reply.mutable_p2_decision()->mutable_txn_digest() = *txnDigest;

    if (signedMessages) {
      proto::Phase2Decision p2Decision(phase2Reply.p2_decision());
      SignMessage(p2Decision, keyManager->GetPrivateKey(id), id,
          phase2Reply.mutable_signed_p2_decision());
    }
  }

  transport->SendMessage(this, remote, phase2Reply);
  Debug("PHASE2[%s] Sent Phase2Reply.", BytesToHex(*txnDigest, 16).c_str());
}

void Server::HandleWriteback(const TransportAddress &remote,
    const proto::Writeback &msg) {
  const proto::Transaction *txn;
  const std::string *txnDigest;
  std::string computedTxnDigest;
  if (!msg.has_txn() && !msg.has_txn_digest()) {
    Debug("WRITEBACK message contains neither txn nor txn_digest.");
    return;
  } 

  if (msg.has_txn_digest()) {
    auto txnItr = ongoing.find(msg.txn_digest());
    if (txnItr == ongoing.end()) {
      Debug("WRITEBACK[%s] message does not contain txn, but have not seen"
          " txn_digest previously.", BytesToHex(msg.txn_digest(), 16).c_str());
      return;
    }

    txn = &txnItr->second;
    txnDigest = &msg.txn_digest();
  } else {
    txn = &msg.txn();
    computedTxnDigest = TransactionDigest(msg.txn(), hashDigest);
    txnDigest = &computedTxnDigest;
  }

  Debug("WRITEBACK[%s] with decision %d.",
      BytesToHex(*txnDigest, 16).c_str(), msg.decision());

  proto::CommittedProof proof;
  if (validateProofs) {
    if (signedMessages && msg.decision() == proto::COMMIT && msg.has_p1_sigs()) {
      if (!ValidateP1Replies(proto::COMMIT, true, txn, txnDigest, msg.p1_sigs(),
            keyManager, &config)) {
        Debug("WRITEBACK[%s] Failed to validate P1 replies for fast commit.",
            BytesToHex(*txnDigest, 16).c_str());
        return;
      }

      *proof.mutable_p1_sigs() = msg.p1_sigs();
    } else if (signedMessages && msg.has_p2_sigs()) {
      if (!ValidateP2Replies(msg.decision(), txnDigest, msg.p2_sigs(),
            keyManager, &config)) {
        Debug("WRITEBACK[%s] Failed to validate P2 replies for decision %d.",
            BytesToHex(*txnDigest, 16).c_str(), msg.decision());
        return;
      }

      if (msg.decision() == proto::COMMIT) {
        *proof.mutable_p2_sigs() = msg.p2_sigs();
      }
    } else if (msg.decision() == proto::ABORT && msg.has_conflict()) {
      std::string committedTxnDigest = TransactionDigest(msg.conflict().txn(),
          hashDigest);
      if (!ValidateCommittedConflict(msg.conflict(), &committedTxnDigest, txn,
            txnDigest, signedMessages, keyManager, &config)) {
        Debug("WRITEBACK[%s] Failed to validate committed conflict for fast abort.",
            BytesToHex(*txnDigest, 16).c_str());
        return;
      }
    } else if (signedMessages) {
      Debug("WRITEBACK[%s] decision %d, has_p1_sigs %d, has_p2_sigs %d, and"
          " has_conflict %d.", BytesToHex(*txnDigest, 16).c_str(),
          msg.decision(), msg.has_p1_sigs(), msg.has_p2_sigs(), msg.has_conflict());
      return;
    }
  }

  if (msg.decision() == proto::COMMIT) {
    if (validateProofs) {
      *proof.mutable_txn() = *txn;
    }
    Debug("WRITEBACK[%s] successfully aborting.", BytesToHex(*txnDigest, 16).c_str());
    Commit(*txnDigest, *txn, proof);
  } else {
    Debug("WRITEBACK[%s] successfully aborting.", BytesToHex(*txnDigest, 16).c_str());
    Abort(*txnDigest);
  }
}

void Server::HandleAbort(const TransportAddress &remote,
    const proto::Abort &msg) {
  const proto::AbortInternal *abort;
  if (validateProofs && signedMessages) {
    if (!msg.has_signed_internal()) {
      return;
    }

    if (!crypto::Verify(keyManager->GetPublicKey(msg.signed_internal().process_id()),
          msg.signed_internal().data(),
          msg.signed_internal().signature())) {
      return;
    }

    if (!abortInternal.ParseFromString(msg.signed_internal().data())) {
      return;
    }

    if (abortInternal.ts().id() != msg.signed_internal().process_id()) {
      return;
    }

    abort = &abortInternal;
  } else {
    UW_ASSERT(msg.has_internal());
    abort = &msg.internal();
  }

  for (const auto &read : abort->read_set()) {
    rts[read].erase(abort->ts());
  }
}

proto::ConcurrencyControl::Result Server::DoOCCCheck(
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
      return proto::ConcurrencyControl::ABORT;
  }
}

proto::ConcurrencyControl::Result Server::DoTAPIROCCCheck(
    const std::string &txnDigest, const proto::Transaction &txn,
    Timestamp &retryTs) {
  Debug("[%s] START PREPARE", txnDigest.c_str());

  if (prepared.find(txnDigest) != prepared.end()) {
    if (prepared[txnDigest].first == txn.timestamp()) {
      Warning("[%s] Already Prepared!", txnDigest.c_str());
      return proto::ConcurrencyControl::COMMIT;
    } else {
      // run the checks again for a new timestamp
      Clean(txnDigest);
    }
  }

  // do OCC checks
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
      if (preparedWrites.find(read.key()) != preparedWrites.end()) {
        Debug("[%lu,%lu] ABSTAIN rw conflict w/ prepared key %s.",
            txn.client_id(),
            txn.client_seq_num(),
            BytesToHex(read.key(), 16).c_str());
        stats.Increment("cc_abstains", 1);
        stats.Increment("cc_abstains_rw_conflict", 1);
        return proto::ConcurrencyControl::ABSTAIN;
      }
    } else {
      // if value is not still valtxnDigest, then abort.
      /*if (Timestamp(txn.timestamp()) <= range.first) {
        Warning("timestamp %lu <= range.first %lu (range.second %lu)",
            txn.timestamp().timestamp(), range.first.getTimestamp(),
            range.second.getTimestamp());
      }*/
      //UW_ASSERT(timestamp > range.first);
      Debug("[%s] ABORT rw conflict: %lu > %lu", txnDigest.c_str(),
          txn.timestamp().timestamp(), range.second.getTimestamp());
      stats.Increment("cc_aborts", 1);
      stats.Increment("cc_aborts_rw_conflict", 1);
      return proto::ConcurrencyControl::ABORT;
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
        stats.Increment("cc_retries_committed_write", 1);
        return proto::ConcurrencyControl::ABSTAIN;
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
        return proto::ConcurrencyControl::ABSTAIN;
      }
    }

    // if there is a pending write for this key, greater than the
    // proposed timestamp, retry
    if (preparedWrites.find(write.key()) != preparedWrites.end()) {
      std::map<Timestamp, const proto::Transaction *>::iterator it =
          preparedWrites[write.key()].upper_bound(txn.timestamp());
      if (it != preparedWrites[write.key()].end() ) {
        Debug("[%s] RETRY ww conflict w/ prepared key:%s", txnDigest.c_str(),
            write.key().c_str());
        retryTs = it->first;
        stats.Increment("cc_retries_prepared_write", 1);
        return proto::ConcurrencyControl::ABSTAIN;
      }
    }

    //if there is a pending read for this key, greater than the
    //propsed timestamp, abstain
    if (pReads.find(write.key()) != pReads.end() &&
        pReads[write.key()].upper_bound(txn.timestamp()) !=
        pReads[write.key()].end()) {
      Debug("[%s] ABSTAIN wr conflict w/ prepared key: %s",
            txnDigest.c_str(), write.key().c_str());
      stats.Increment("cc_abstains", 1);
      return proto::ConcurrencyControl::ABSTAIN;
    }
  }

  // Otherwise, prepare this transaction for commit
  Prepare(txnDigest, txn);

  Debug("[%s] PREPARED TO COMMIT", txnDigest.c_str());

  return proto::ConcurrencyControl::COMMIT;
}

proto::ConcurrencyControl::Result Server::DoMVTSOOCCCheck(
    uint64_t reqId, const TransportAddress &remote,
    const std::string &txnDigest, const proto::Transaction &txn,
    proto::CommittedProof &conflict) {
  Debug("PREPARE[%lu:%lu][%s] with ts %lu.%lu.",
      txn.client_id(), txn.client_seq_num(),
      BytesToHex(txnDigest, 16).c_str(),
      txn.timestamp().timestamp(), txn.timestamp().id());
  Timestamp ts(txn.timestamp());
  if (CheckHighWatermark(ts)) {
    Debug("[%lu:%lu][%s] ABSTAIN ts %lu beyond high watermark.",
        txn.client_id(), txn.client_seq_num(),
        BytesToHex(txnDigest, 16).c_str(),
        ts.getTimestamp());
    stats.Increment("cc_abstains", 1);
    stats.Increment("cc_abstains_watermark", 1);
    return proto::ConcurrencyControl::ABSTAIN;
  }

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
          conflict = *committedWrite.second.proof;
        } 
        Debug("[%lu:%lu][%s] ABORT wr conflict committed write for key %s:"
            " this txn's read ts %lu.%lu < committed ts %lu.%lu < this txn's ts %lu.%lu.",
            txn.client_id(),
            txn.client_seq_num(),
            BytesToHex(txnDigest, 16).c_str(),
            BytesToHex(read.key(), 16).c_str(),
            read.readtime().timestamp(),
            read.readtime().id(), committedWrite.first.getTimestamp(),
            committedWrite.first.getID(), ts.getTimestamp(), ts.getID());
        stats.Increment("cc_aborts", 1);
        stats.Increment("cc_aborts_wr_conflict", 1);
        return proto::ConcurrencyControl::ABORT;
      }
    }

    const auto preparedWritesItr = preparedWrites.find(read.key());
    if (preparedWritesItr != preparedWrites.end()) {
      for (const auto &preparedTs : preparedWritesItr->second) {
        if (Timestamp(read.readtime()) < preparedTs.first && preparedTs.first < ts) {
          Debug("[%lu:%lu][%s] ABSTAIN wr conflict prepared write for key %s:"
            " this txn's read ts %lu.%lu < prepared ts %lu.%lu < this txn's ts %lu.%lu.",
              txn.client_id(),
              txn.client_seq_num(),
              BytesToHex(txnDigest, 16).c_str(),
              BytesToHex(read.key(), 16).c_str(),
              read.readtime().timestamp(),
              read.readtime().id(), preparedTs.first.getTimestamp(),
              preparedTs.first.getID(), ts.getTimestamp(), ts.getID());
          stats.Increment("cc_abstains", 1);
          stats.Increment("cc_abstains_wr_conflict", 1);
          return proto::ConcurrencyControl::ABSTAIN;
        }
      }
    }
  }
  
  for (const auto &write : txn.write_set()) {
    if (!IsKeyOwned(write.key())) {
      continue;
    }

    auto committedReadsItr = committedReads.find(write.key());

    if (committedReadsItr != committedReads.end() && committedReadsItr->second.size() > 0) {
      for (auto ritr = committedReadsItr->second.rbegin();
          ritr != committedReadsItr->second.rend(); ++ritr) {
        if (ts >= std::get<0>(*ritr)) {
          // iterating over committed reads from largest to smallest committed txn ts
          //    if ts is larger than current itr, it is also larger than all subsequent itrs
          break;
        } else if (std::get<1>(*ritr) < ts) {
          if (validateProofs) {
            conflict = *std::get<2>(*ritr);
          } 
          Debug("[%lu:%lu][%s] ABORT rw conflict committed read for key %s: committed"
              " read ts %lu.%lu < this txn's ts %lu.%lu < committed ts %lu.%lu.",
              txn.client_id(),
              txn.client_seq_num(),
              BytesToHex(txnDigest, 16).c_str(),
              BytesToHex(write.key(), 16).c_str(),
              std::get<1>(*ritr).getTimestamp(),
              std::get<1>(*ritr).getID(), ts.getTimestamp(),
              ts.getID(), std::get<0>(*ritr).getTimestamp(),
              std::get<0>(*ritr).getID());
          stats.Increment("cc_aborts", 1);
          stats.Increment("cc_aborts_rw_conflict", 1);
          return proto::ConcurrencyControl::ABORT;
        }
      }
    }

    const auto preparedReadsItr = preparedReads.find(write.key());
    if (preparedReadsItr != preparedReads.end()) {
      for (const auto preparedReadTxn : preparedReadsItr->second) {
        bool isDep = false;
        for (const auto &dep : preparedReadTxn->deps()) {
          if (txnDigest == dep.prepared().txn_digest()) {
            isDep = true;
            break;
          }
        }

        bool isReadVersionEarlier = false;
        Timestamp readTs;
        for (const auto &read : preparedReadTxn->read_set()) {
          if (read.key() == write.key()) {
            readTs = Timestamp(read.readtime());
            isReadVersionEarlier = readTs < ts;
            break;
          }
        }
        if (!isDep && isReadVersionEarlier &&
            ts < Timestamp(preparedReadTxn->timestamp())) {
          Debug("[%lu:%lu][%s] ABSTAIN rw conflict prepared read for key %s: prepared"
              " read ts %lu.%lu < this txn's ts %lu.%lu < committed ts %lu.%lu.",
              txn.client_id(),
              txn.client_seq_num(),
              BytesToHex(txnDigest, 16).c_str(),
              BytesToHex(write.key(), 16).c_str(),
              readTs.getTimestamp(),
              readTs.getID(), ts.getTimestamp(),
              ts.getID(), preparedReadTxn->timestamp().timestamp(),
              preparedReadTxn->timestamp().id());
          stats.Increment("cc_abstains", 1);
          stats.Increment("cc_abstains_rw_conflict", 1);
          return proto::ConcurrencyControl::ABSTAIN;
        }
      }
    }

    auto rtsItr = rts.find(write.key());
    if (rtsItr != rts.end()) {
      auto rtsRBegin = rtsItr->second.rbegin();
      if (rtsRBegin != rtsItr->second.rend()) {
        Debug("Largest rts for write to key %s: %lu.%lu.",
          BytesToHex(write.key(), 16).c_str(), rtsRBegin->getTimestamp(),
          rtsRBegin->getID());
      }
      auto rtsLB = rtsItr->second.lower_bound(ts);
      if (rtsLB != rtsItr->second.end()) {
        Debug("Lower bound rts for write to key %s: %lu.%lu.",
          BytesToHex(write.key(), 16).c_str(), rtsLB->getTimestamp(),
          rtsLB->getID());
        if (*rtsLB == ts) {
          rtsLB++;
        }
        if (rtsLB != rtsItr->second.end()) {
          if (*rtsLB > ts) {
            Debug("[%lu:%lu][%s] ABSTAIN larger rts acquired for key %s: rts %lu.%lu >"
                " this txn's ts %lu.%lu.",
                txn.client_id(),
                txn.client_seq_num(),
                BytesToHex(txnDigest, 16).c_str(),
                BytesToHex(write.key(), 16).c_str(),
                rtsLB->getTimestamp(),
                rtsLB->getID(), ts.getTimestamp(), ts.getID());
            stats.Increment("cc_abstains", 1);
            stats.Increment("cc_abstains_rts", 1);
            return proto::ConcurrencyControl::ABSTAIN;
          }
        }
      }
    }
    // TODO: add additional rts dep check to shrink abort window
    //    Is this still a thing?
  }

  Prepare(txnDigest, txn);

  bool allFinished = true;
  for (const auto &dep : txn.deps()) {
    if (dep.involved_group() != groupIdx) {
      continue;
    }
    if (committed.find(dep.prepared().txn_digest()) == committed.end() &&
        aborted.find(dep.prepared().txn_digest()) == aborted.end()) {
      Debug("[%lu:%lu][%s] WAIT for dependency %s to finish.",
          txn.client_id(), txn.client_seq_num(),
          BytesToHex(txnDigest, 16).c_str(),
          BytesToHex(dep.prepared().txn_digest(), 16).c_str());
      allFinished = false;
      dependents[dep.prepared().txn_digest()].insert(txnDigest);
      auto dependenciesItr = waitingDependencies.find(txnDigest);
      if (dependenciesItr == waitingDependencies.end()) {
        auto inserted = waitingDependencies.insert(std::make_pair(txnDigest,
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
    stats.Increment("cc_waits", 1);
    return proto::ConcurrencyControl::WAIT;
  } else {
    return CheckDependencies(txn);
  }
}

void Server::GetPreparedWriteTimestamps(
    std::unordered_map<std::string, std::set<Timestamp>> &writes) {
  // gather up the set of all writes that are currently prepared
  for (const auto &t : prepared) {
    for (const auto &write : t.second.second->write_set()) {
      if (IsKeyOwned(write.key())) {
        writes[write.key()].insert(t.second.first);
      }
    }
  }
}

void Server::GetPreparedWrites(
      std::unordered_map<std::string, std::vector<const proto::Transaction *>> &writes) {
  for (const auto &t : prepared) {
    for (const auto &write : t.second.second->write_set()) {
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
    for (const auto &read : t.second.second->read_set()) {
      if (IsKeyOwned(read.key())) {
        reads[read.key()].insert(t.second.first);
      }
    }
  }
}

void Server::GetPreparedReads(
    std::unordered_map<std::string, std::vector<const proto::Transaction*>> &reads) {
  // gather up the set of all writes that are currently prepared
  for (const auto &t : prepared) {
    for (const auto &read : t.second.second->read_set()) {
      if (IsKeyOwned(read.key())) {
        reads[read.key()].push_back(t.second.second);
      }
    }
  }
}

void Server::Prepare(const std::string &txnDigest,
    const proto::Transaction &txn) {
  Debug("PREPARE[%s] agreed to commit with ts %lu.%lu.",
      BytesToHex(txnDigest, 16).c_str(), txn.timestamp().timestamp(), txn.timestamp().id());
  proto::Transaction &ongoingTxn= ongoing.at(txnDigest);
  auto p = prepared.insert(std::make_pair(txnDigest, std::make_pair(
          Timestamp(txn.timestamp()), &ongoingTxn)));
  for (const auto &read : txn.read_set()) {
    if (IsKeyOwned(read.key())) {
      preparedReads[read.key()].insert(p.first->second.second);
    }
  }
  std::pair<Timestamp, const proto::Transaction *> pWrite =
    std::make_pair(p.first->second.first, p.first->second.second);
  for (const auto &write : txn.write_set()) {
    if (IsKeyOwned(write.key())) {
      preparedWrites[write.key()].insert(pWrite);
    }
  }
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

void Server::Commit(const std::string &txnDigest,
    const proto::Transaction &txn, const proto::CommittedProof &proof) {
  Timestamp ts(txn.timestamp());

  auto committedItr = committed.insert(std::make_pair(txnDigest, proof));

  Value val;
  if (validateProofs) {
    UW_ASSERT(proof.has_txn());
    val.proof = &committedItr.first->second;
  }

  for (const auto &read : txn.read_set()) {
    if (!IsKeyOwned(read.key())) {
      continue;
    }
    store.commitGet(read.key(), read.readtime(), ts);
    //Latency_Start(&committedReadInsertLat);
    committedReads[read.key()].insert(std::make_tuple(ts, read.readtime(), &committedItr.first->second));
    //uint64_t ns = Latency_End(&committedReadInsertLat);
    //stats.Add("committed_read_insert_lat_" + BytesToHex(read.key(), 18), ns);
  }
  

  for (const auto &write : txn.write_set()) {
    if (!IsKeyOwned(write.key())) {
      continue;
    }

    Debug("COMMIT[%lu,%lu] Committing write for key %s.",
        txn.client_id(), txn.client_seq_num(),
        BytesToHex(write.key(), 16).c_str());
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

  Clean(txnDigest);
  CheckDependents(txnDigest);
  CleanDependencies(txnDigest);
}

void Server::Abort(const std::string &txnDigest) {
  aborted.insert(txnDigest);
  Clean(txnDigest);
  CheckDependents(txnDigest);
  CleanDependencies(txnDigest);
}

void Server::Clean(const std::string &txnDigest) {
  auto itr = prepared.find(txnDigest);
  if (itr != prepared.end()) {
    for (const auto &read : itr->second.second->read_set()) {
      preparedReads[read.key()].erase(itr->second.second);
    }
    for (const auto &write : itr->second.second->write_set()) {
      preparedWrites[write.key()].erase(itr->second.first);
    }
    prepared.erase(itr);
  }
  ongoing.erase(txnDigest);
}

void Server::CheckDependents(const std::string &txnDigest) {
  auto dependentsItr = dependents.find(txnDigest);
  if (dependentsItr != dependents.end()) {
    for (const auto &dependent : dependentsItr->second) {
      auto dependenciesItr = waitingDependencies.find(dependent);
      UW_ASSERT(dependenciesItr != waitingDependencies.end());

      dependenciesItr->second.deps.erase(txnDigest);
      if (dependenciesItr->second.deps.size() == 0) {
        proto::ConcurrencyControl::Result result = CheckDependencies(
            dependent);
        UW_ASSERT(result != proto::ConcurrencyControl::ABORT);
        waitingDependencies.erase(dependent);
        proto::CommittedProof conflict;
        SendPhase1Reply(dependenciesItr->second.reqId, result, conflict, dependent,
            *dependenciesItr->second.remote);
      }
    }
  }
}

proto::ConcurrencyControl::Result Server::CheckDependencies(
    const std::string &txnDigest) {
  auto txnItr = ongoing.find(txnDigest);
  UW_ASSERT(txnItr != ongoing.end());
  return CheckDependencies(txnItr->second);
}

proto::ConcurrencyControl::Result Server::CheckDependencies(
    const proto::Transaction &txn) {
  for (const auto &dep : txn.deps()) {
    if (dep.involved_group() != groupIdx) {
      continue;
    }
    if (committed.find(dep.prepared().txn_digest()) != committed.end()) {
      if (Timestamp(dep.prepared().timestamp()) > Timestamp(txn.timestamp())) {
        stats.Increment("cc_aborts", 1);
        stats.Increment("cc_aborts_dep_ts", 1);
        return proto::ConcurrencyControl::ABSTAIN;
      }
    } else {
      stats.Increment("cc_aborts", 1);
      stats.Increment("cc_aborts_dep_aborted", 1);
      return proto::ConcurrencyControl::ABSTAIN;
    }
  } 
  return proto::ConcurrencyControl::COMMIT;
}

bool Server::CheckHighWatermark(const Timestamp &ts) {
  Timestamp highWatermark(timeServer.GetTime());
  // add delta to current local time
  highWatermark.setTimestamp(highWatermark.getTimestamp() + timeDelta);
  Debug("High watermark: %lu.", highWatermark.getTimestamp());
  return ts > highWatermark;
}

void Server::SendPhase1Reply(uint64_t reqId,
    proto::ConcurrencyControl::Result result,
    const proto::CommittedProof &conflict, const std::string &txnDigest,
    const TransportAddress &remote) {
  phase1Reply.Clear();
  phase1Reply.set_req_id(reqId);

  phase1Reply.mutable_cc()->set_ccr(result);
  if (validateProofs) {
    *phase1Reply.mutable_cc()->mutable_txn_digest() = txnDigest;
    if (result == proto::ConcurrencyControl::ABORT) {
      *phase1Reply.mutable_cc()->mutable_committed_conflict() = conflict;
    } else if (signedMessages) {
      proto::ConcurrencyControl cc(phase1Reply.cc());
      SignMessage(cc, keyManager->GetPrivateKey(id), id,
          phase1Reply.mutable_signed_cc());
    }
  }

  transport->SendMessage(this, remote, phase1Reply);
}

void Server::CleanDependencies(const std::string &txnDigest) {
  auto dependenciesItr = waitingDependencies.find(txnDigest);
  if (dependenciesItr != waitingDependencies.end()) {
    for (const auto &dependency : dependenciesItr->second.deps) {
      auto dependentItr = dependents.find(dependency);
      if (dependentItr != dependents.end()) {
        dependentItr->second.erase(txnDigest);
      }
    }
    waitingDependencies.erase(dependenciesItr);
  }
  dependents.erase(txnDigest);
}

} // namespace indicusstore
