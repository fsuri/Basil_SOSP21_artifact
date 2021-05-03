#include "store/bftsmartstore_stable/server.h"
#include "store/bftsmartstore_stable/common.h"
#include "store/common/transaction.h"
#include <iostream>
#include <sys/time.h>

namespace bftsmartstore_stable {

using namespace std;

Server::Server(const transport::Configuration& config, KeyManager *keyManager,
  int groupIdx, int idx, int numShards, int numGroups, bool signMessages,
  bool validateProofs, uint64_t timeDelta, Partitioner *part, Transport* tp,
  bool order_commit, bool validate_abort,
  TrueTime timeServer) : config(config), keyManager(keyManager),
  groupIdx(groupIdx), idx(idx), id(groupIdx * config.n + idx),
  numShards(numShards), numGroups(numGroups), signMessages(signMessages),
  validateProofs(validateProofs),  timeDelta(timeDelta), part(part), tp(tp),
  order_commit(order_commit), validate_abort(validate_abort),
  timeServer(timeServer) {
  dummyProof = std::make_shared<proto::CommitProof>();

  dummyProof->mutable_writeback_message()->set_status(REPLY_OK);
  dummyProof->mutable_writeback_message()->set_txn_digest("");
  proto::ShardSignedDecisions dec;
  *dummyProof->mutable_writeback_message()->mutable_signed_decisions() = dec;

  dummyProof->mutable_txn()->mutable_timestamp()->set_timestamp(0);
  dummyProof->mutable_txn()->mutable_timestamp()->set_id(0);


  decision_test = new proto::TransactionDecision();
  decision_test->set_txn_digest(std::string("dummy digest"));
  decision_test->set_shard_id(groupIdx);
  decision_test->set_status(REPLY_OK);

  for(int i = 0; i < 0; i++){
    proto::ReadResult *readResult = decision_test->add_readset();

    readResult->set_key(std::string("hello"));
    //std::cerr << "read key size: " << readResult->key().length() << std::endl;
    readResult->set_value("");
    test_vector.push_back(*readResult);
  }



}

Server::~Server() {}

//WARNING: UNSAFE WITH STORED PROCEDURE READS
bool Server::CCC2(const proto::Transaction& txn) {
  Debug("Starting ccc v2 check");
  Timestamp txTs(txn.timestamp());
  for (const auto &read : txn.readset()) {
    if(!IsKeyOwned(read.key())) {
      continue;
    }

    // we want to make sure that our reads don't span any
    // committed/prepared writes

    // check the committed writes
    Timestamp rts(read.readtime());
    // we want to make sure there are no committed writes for this key after
    // the rts and before the txTs
    std::vector<std::pair<Timestamp, Server::ValueAndProof>> committedWrites;
    if (commitStore.getCommittedAfter(read.key(), rts, committedWrites)) {
      for (const auto& committedWrite : committedWrites) {
        if (committedWrite.first < txTs) {
          Debug("found committed conflict with read for key: %s", read.key().c_str());
          return false;
        }
      }
    }

    // check the prepared writes
    const auto preparedWritesItr = preparedWrites.find(read.key());
    if (preparedWritesItr != preparedWrites.end()) {
      for (const auto& writeTs : preparedWritesItr->second) {
        if (rts < writeTs && writeTs < txTs) {
          Debug("found prepared conflict with read for key: %s", read.key().c_str());
          return false;
        }
      }
    }

  }

  Debug("checked all reads");

  for (const auto &write : txn.writeset()) {
    if(!IsKeyOwned(write.key())) {
      continue;
    }

    // we want to make sure that no prepared/committed read spans
    // our writes

    // check commited reads
    // for (const auto& read : committedReads[write.key()]) {
    //   // second is the read ts, first is the txTs that did the read
    //   if (read.second < txTs && txTs < read.first) {
    //       Debug("found committed conflict with write for key: %s", write.key().c_str());
    //       return false;
    //     }
    //   }

    auto committedReadsItr = committedReads.find(write.key());

    if (committedReadsItr != committedReads.end() && committedReadsItr->second.size() > 0) {

      for (auto read = committedReadsItr->second.rbegin(); read != committedReadsItr->second.rend(); ++read) {
      //for (const auto& read : committedReads[write.key()]) {
        // second is the read ts, first is the txTs that did the read
        if (txTs >= read->first){
          break;
        }
        if (read->second < txTs && txTs < read->first) {
            Debug("found committed conflict with write for key: %s", write.key().c_str());
            return false;
        }

      }
    }

    // check prepared reads
    for (const auto& read : preparedReads[write.key()]) {
      // second is the read ts, first is the txTs that did the read
      if (read.second < txTs && txTs < read.first) {
          Debug("found prepared conflict with write for key: %s", write.key().c_str());
          return false;
      }
    }
  }
  return true;
}

bool Server::CCC(const proto::Transaction& txn) {
  Debug("Starting ccc check");
  Timestamp txTs(txn.timestamp());
  for (const auto &read : txn.readset()) {
    if(!IsKeyOwned(read.key())) {
      continue;
    }

    // we want to make sure that our reads don't span any
    // committed/prepared writes

    // check the committed writes
    Timestamp rts(read.readtime());
    Timestamp upper;
    // this is equivalent to checking if there is a write with a timestamp t
    // such that t > rts and t < txTs
    if (commitStore.getUpperBound(read.key(), rts, upper)) {
      if (upper < txTs) {
        Debug("found committed conflict with read for key: %s", read.key().c_str());
        return false;
      }
    }

    // check the prepared writes
    for (const auto& pair : pendingTransactions) {
      for (const auto& write : pair.second.writeset()) {
        if (write.key() == read.key()) {
          Timestamp wts(pair.second.timestamp());
          if (wts > rts && wts < txTs) {
            Debug("found prepared conflict with read for key: %s", read.key().c_str());
            return false;
          }
        }
      }
    }
  }

  Debug("checked all reads");

  for (const auto &write : txn.writeset()) {
    if(!IsKeyOwned(write.key())) {
      continue;
    }

    // we want to make sure that no prepared/committed read spans
    // our writes

    // check commited reads
    // get a pointer to the first read that commits after this tx
    auto it = committedReads[write.key()].lower_bound(txTs);
    if (it != committedReads[write.key()].end()) {
      // if the iterator is at the end, then that means there are no committed reads
      // before this tx
      it++;
      // all iterator pairs committed after txTs (commit ts > txTs)
      // so we just need to check if they returned a version before txTs (read ts < txTs)
      while(it != committedReads[write.key()].end()) {
        if ((*it).second < txTs) {
          Debug("found committed conflict with write for key: %s", write.key().c_str());
          return false;
        }
        it++;
      }
    }

    // next, check the prepared tx's read sets
    for (const auto& pair : pendingTransactions) {
      for (const auto& read : pair.second.readset()) {
        if (read.key() == write.key()) {
          Timestamp pendingTxTs(pair.second.timestamp());
          Timestamp rts(read.readtime());
          if (txTs > rts && txTs < pendingTxTs) {
            Debug("found prepared conflict with write for key: %s", write.key().c_str());
            return false;
          }
        }
      }
    }
  }
  return true;

}

::google::protobuf::Message* Server::returnMessage(::google::protobuf::Message* msg) {
  // Send decision to client
  if (signMessages) {
    proto::SignedMessage *signedMessage = new proto::SignedMessage();
    SignMessage(*msg, keyManager->GetPrivateKey(id), id, *signedMessage);
    delete msg;
    return signedMessage;
  } else {
    return msg;
  }
}

std::vector<::google::protobuf::Message*> Server::Execute(const string& type, const string& msg) {
  Debug("Execute: %s", type.c_str());
  //std::unique_lock lock(atomicMutex);

  proto::Transaction transaction;
  proto::GroupedDecision gdecision;
  if (type == transaction.GetTypeName()) {
    transaction.ParseFromString(msg);

    return HandleTransaction(transaction);
  } else if (type == gdecision.GetTypeName()) {
    gdecision.ParseFromString(msg);

    if (gdecision.status() == REPLY_FAIL) {
      std::vector<::google::protobuf::Message*> results;
      results.push_back(HandleGroupedAbortDecision(gdecision));
      return results;
    } else if(order_commit && gdecision.status() == REPLY_OK) {
      std::vector<::google::protobuf::Message*> results;
      results.push_back(HandleGroupedCommitDecision(gdecision));
      return results;
    }
    else{
      Panic("Only failed grouped decisions should be atomically broadcast");
    }
  }
  std::vector<::google::protobuf::Message*> results;
  results.push_back(nullptr);
  return results;
}

std::vector<::google::protobuf::Message*> Server::HandleTransaction(const proto::Transaction& transaction) {
  std::unique_lock lock(atomicMutex); //TODO: might be able to make it finer.

  std::vector<::google::protobuf::Message*> results;
  proto::TransactionDecision* decision = new proto::TransactionDecision();
  //std::cerr << "allocating reply" << std::endl;

  string digest = TransactionDigest(transaction);
  Debug("Handling transaction");
  DebugHash(digest);
  stats.Increment("handle_tx",1);
  decision->set_txn_digest(digest);
  decision->set_shard_id(groupIdx);
  //decision_test->set_txn_digest(digest);


  if (bufferedGDecs.find(digest) != bufferedGDecs.end()) {
    pendingTransactions[digest] = std::move(transaction);
    stats.Increment("used_buffered_gdec",1);
    Debug("found buffered gdecision");
    if(bufferedGDecs[digest].status() == REPLY_OK){
      //std::cerr <<" trying to call HandleGroupedCommitDecision while holding lock for txn: " << BytesToHex(digest, 16) << std::endl;
      results.push_back(HandleGroupedCommitDecision(bufferedGDecs[digest], false));
    }
    else{
      results.push_back(HandleGroupedAbortDecision(bufferedGDecs[digest]));
    }
    bufferedGDecs.erase(digest);
    return results;
  }

  // OCC check
  bool commit = std::rand() % 100 < 55;
  if(augustus.TryLock(transaction, digest, this)){
  //if (CCC2(transaction)) {
    stats.Increment("ccc_succeed",1);
    Debug("ccc succeeded");
    decision->set_status(REPLY_OK);


    proto::TransactionDecision* decisio2 = new proto::TransactionDecision();
    int ct = 0;
    for (const auto &read : transaction.readset()) {
        ct++;
      if(!IsKeyOwned(read.key())) {
        continue;
      }
      stats.Increment("apply_augustus_tx_read",1);

      //attach read result to the decision message back to client

      proto::ReadResult *readResult = decision->add_readset();

      // proto::ReadResult *rR;
      // if(ct == 1) rR = decision->mutable_r1();
      // if(ct == 2) rR = decision->mutable_r2();
      // if(ct == 3) rR = decision->mutable_r3();

      readResult->set_key(read.key());
      //std::cerr << "read key size: " << readResult->key().length() << std::endl;
      if (augustus.store.count(read.key())) {
          readResult->set_value(augustus.store[read.key()]);
          //rR->set_value(augustus.store[read.key()]);
      } else {
          readResult->set_value("");
          //rR->set_value("");
      }
      // std::string* rd = decision->mutable_rs()->add_keys();
      // std::string* write = decision->mutable_rs()->add_values();
      // *rd = read.key();
      // *write = augustus.store[read.key()];


      // if(ct % 100 ==0) {
      //   std::string val = readResult->value();
      //   std::cerr << "+1 read value of size " << val.length() << std::endl;
      // }
    }


    // Augustus single-shard optimization
    if (1 == transaction.participating_shards_size()) {
        Debug("applying tx");
        for (const auto &write : transaction.writeset()) {
            if(!IsKeyOwned(write.key())) {
                continue;
            }
            augustus.store[write.key()] = write.value();
            stats.Increment("apply_augustus_tx_write",1);
        }

        // mark txn as commited
        pendingTransactions[digest] = std::move(transaction);
        cleanupPendingTx(digest);
        stats.Increment("augustus_optimized",1);
    } else {
        stats.Increment("augustus_not_optimized",1);
        pendingTransactions[digest] = std::move(transaction);
    }

  } else {
    Debug("ccc failed");
    stats.Increment("ccc_fail",1);
    decision->set_status(REPLY_FAIL);

    // Augustus client recovery
    // the transaction is not the conflict transaction, but it doesn't matter since we just want a field in the reply message and we won't use it
    *(decision->mutable_conflict()) = transaction;

    pendingTransactions[digest] = std::move(transaction);

    // if (bufferedGDecs.find(digest) != bufferedGDecs.end()) {
    //   stats.Increment("used_buffered_gdec",1);
    //   Debug("found buffered gdecision");
    //   results.push_back(HandleGroupedAbortDecision(bufferedGDecs[digest]));
    //   bufferedGDecs.erase(digest);
    // }
  }

  //*decision->mutable_readset() = {test_vector.begin(), test_vector.end()};
  results.push_back(decision);

  return results;
}

::google::protobuf::Message* Server::HandleMessage(const string& type, const string& msg) {
  Debug("Handle %s", type.c_str());
  //std::shared_lock lock(atomicMutex);
  //std::unique_lock lock(atomicMutex);

  proto::Read read;
  proto::GroupedDecision gdecision;

  if (type == read.GetTypeName()) {
    read.ParseFromString(msg);

    return HandleRead(read);
  } else if (type == gdecision.GetTypeName()) {
    if(order_commit && signMessages){
      Panic("Should be ordering all Writeback messages");
    }
    gdecision.ParseFromString(msg);
    if (gdecision.status() == REPLY_OK) {
      //std::unique_lock lock(atomicMutex);
      return HandleGroupedCommitDecision(gdecision);
    } else {
      Panic("Only commit grouped decisions allowed to be sent directly to server");
    }

  }
  else{
    Panic("Request not of type Read (or Commit Writeback)");
  }

  return nullptr;
}

::google::protobuf::Message* Server::HandleRead(const proto::Read& read) {
  Timestamp ts(read.timestamp());
  //std::shared_lock lock(atomicMutex); //come back to this: probably dont need it at all.

  stats.Increment("total_reads_processed", 1);
  pair<Timestamp, ValueAndProof> result;
  bool exists = commitStore.get(read.key(), ts, result);

  proto::ReadReply* readReply = new proto::ReadReply();
  Debug("Handle read req id %lu", read.req_id());
  readReply->set_req_id(read.req_id());
  readReply->set_key(read.key());
  if (exists) {
    Debug("Read exists f");
    Debug("Read exits for key: %s  value: %s", read.key().c_str(), result.second.value.c_str());
    readReply->set_status(REPLY_OK);
    readReply->set_value(result.second.value);
    result.first.serialize(readReply->mutable_value_timestamp());
    if (validateProofs) {
      *readReply->mutable_commit_proof() = *result.second.commitProof;
    }
  } else {
    stats.Increment("read_dne",1);
    Debug("Read does not exit for key: %s", read.key().c_str());
    readReply->set_status(REPLY_FAIL);
  }
  //lock.unlock_shared();
  return returnMessage(readReply);
}

::google::protobuf::Message* Server::HandleGroupedCommitDecision(const proto::GroupedDecision& gdecision, bool lock) {
  // proto::GroupedDecisionAck* groupedDecisionAck = new proto::GroupedDecisionAck();

  Debug("Handling Grouped commit Decision");
  string digest = gdecision.txn_digest();
  DebugHash(digest);

  //std::cerr <<" called HandleGroupedCommitDecision for txn: " << BytesToHex(digest, 16) << std::endl;
  // groupedDecisionAck->set_txn_digest(digest);
  if(lock) atomicMutex.lock();
  //std::cerr <<" acquired 1st lock in HandleGroupedcommit for txb:" << BytesToHex(digest, 16) << std::endl;
  if (pendingTransactions.find(digest) == pendingTransactions.end()) {

    Debug("Buffering gdecision");
    stats.Increment("buff_dec",1);
    // we haven't yet received the tx so buffer this gdecision until we get it
    bufferedGDecs[digest] = std::move(gdecision);
    if(lock) atomicMutex.unlock();
    //std::cerr << "buffering HandleGroupedCommitDecision for txn: " << BytesToHex(digest, 16)  << std::endl;
    return nullptr;
  }
  if(lock) atomicMutex.unlock();
  //std::cerr <<" released 1st lock in HandleGroupedcommit for txb:" << BytesToHex(digest, 16) << std::endl;
  // verify gdecision

    // struct timeval tp;
    // gettimeofday(&tp, NULL);
    // long int us = tp.tv_sec * 1000 * 1000 + tp.tv_usec;

  if (verifyGDecision_parallel(gdecision, pendingTransactions[digest], keyManager, signMessages, config.f, tp)) {

  //if (verifyGDecision(gdecision, pendingTransactions[digest], keyManager, signMessages, config.f)) {
 //if(true){
    // gettimeofday(&tp, NULL);
    // long int lock_time = ((tp.tv_sec * 1000 * 1000 + tp.tv_usec) -us);
    // std::cerr << "Commit Verification takes " << lock_time << " microseconds" << std::endl;
     //std::cerr <<" about to acquire 2st lock in HandleGroupedcommit for txn:" << BytesToHex(digest, 16) << std::endl;
    std::unique_lock atomic_lock = lock ? std::unique_lock(atomicMutex) : std::unique_lock<std::shared_mutex>();


    //std::unique_lock lock(atomicMutex);
    stats.Increment("apply_tx",1);
    proto::Transaction &txn = pendingTransactions[digest];
    // apply tx
    Debug("applying tx");
    for (const auto &write : txn.writeset()) {
      if(!IsKeyOwned(write.key())) {
        continue;
      }
      augustus.store[write.key()] = write.value();
      stats.Increment("apply_augustus_tx_write",1);
    }


    // mark txn as commited
    cleanupPendingTx(digest);
    // groupedDecisionAck->set_status(REPLY_OK);
  } else {
    stats.Increment("gdec_failed_valid",1);
    // groupedDecisionAck->set_status(REPLY_FAIL);
  }
  //std::cerr << "finishing HandleGroupedCommitDecision normally" << BytesToHex(digest, 16) << std::endl;
  // Debug("decision ack status: %d", groupedDecisionAck->status());

  // return returnMessage(groupedDecisionAck);
  return nullptr;
}


::google::protobuf::Message* Server::HandleGroupedAbortDecision(const proto::GroupedDecision& gdecision) {
  // proto::GroupedDecisionAck* groupedDecisionAck = new proto::GroupedDecisionAck();


  Debug("Handling Grouped abort Decision");
  string digest = gdecision.txn_digest();
  DebugHash(digest);

  atomicMutex.lock();
  if (pendingTransactions.find(digest) == pendingTransactions.end()) {

    Debug("Buffering gdecision");
    stats.Increment("buff_dec",1);
    // we haven't yet received the tx so buffer this gdecision until we get it
    bufferedGDecs[digest] = gdecision;
    atomicMutex.unlock();
    return nullptr;
  }
  atomicMutex.unlock();

 if(validate_abort){
   // struct timeval tp;
   // gettimeofday(&tp, NULL);
   // long int us = tp.tv_sec * 1000 * 1000 + tp.tv_usec;

   if(!verifyG_Abort_Decision(gdecision, pendingTransactions[digest], keyManager, signMessages, config.f)){
    //if(!verifyGDecision_Abort_parallel(gdecision, pendingTransactions[digest], keyManager, signMessages, config.f, tp)){
     Debug("failed validation for abort decision");
     return nullptr;
   }

   // gettimeofday(&tp, NULL);
   // long int lock_time = ((tp.tv_sec * 1000 * 1000 + tp.tv_usec) -us);
   // std::cerr << "Abort Verification takes " << lock_time << " microseconds" << std::endl;
 }
 std::unique_lock lock(atomicMutex);

  // groupedDecisionAck->set_txn_digest(digest);

  stats.Increment("gdec_failed",1);
  // abort the tx
  cleanupPendingTx(digest);
  // there is a chance that this abort comes before we see the tx, so save the decision
  abortedTxs.insert(digest);

  // groupedDecisionAck->set_status(REPLY_FAIL);
  //
  // return returnMessage(groupedDecisionAck);
  return nullptr;
}

void Server::cleanupPendingTx(std::string &digest) {
  if (pendingTransactions.find(digest) != pendingTransactions.end()) {
    proto::Transaction &tx = pendingTransactions[digest];
    // remove prepared reads and writes
    // Timestamp txTs(tx.timestamp());
    // for (const auto& write : tx.writeset()) {
    //   if(!IsKeyOwned(write.key())) {
    //     continue;
    //   }
    //   preparedWrites[write.key()].erase(txTs);
    // }
    // for (const auto& read : tx.readset()) {
    //   if(!IsKeyOwned(read.key())) {
    //     continue;
    //   }
    //   preparedReads[read.key()].erase(txTs);
    // }
    // release Augustus locks

    augustus.ReleaseLock(tx, digest, this);


    pendingTransactions.erase(digest);
  }
}

void Server::Load(const string &key, const string &value,
    const Timestamp timestamp) {
      // if (IsKeyOwned(key)) {
  ValueAndProof val;
  val.value = value;
  val.commitProof = dummyProof;
  commitStore.put(key, val, timestamp);

      // }
}

Stats &Server::GetStats() {
  return stats;
}

Stats* Server::mutableStats() {
  return &stats;
}


/*************************Augustus Logic*************************/

bool Augustus::TryLock(const proto::Transaction& txn, std::string &digest, class Server* server) {
    // check whether all locks are available
    for (const auto &read : txn.readset()) {
      if(!server->IsKeyOwned(read.key())) {
        continue;
      }
      if (locks.count(read.key())) {
          // read lock cannot be concurrent with write lock
          if (locks[read.key()].state == rwLock::LOCKED_WRITE)
              return false;
      } else {
          locks[read.key()] = rwLock();
      }
    }

    for (const auto &write : txn.writeset()) {
      if(!server->IsKeyOwned(write.key())) {
        continue;
      }
      if (locks.count(write.key())) {
          // write lock cannot be concurrent with read or write lock
          if (locks[write.key()].state == rwLock::LOCKED_READ ||
              locks[write.key()].state == rwLock::LOCKED_WRITE)
              return false;
      } else {
          locks[write.key()] = rwLock();
      }
    }

    // all locks are available, acquire the locks

    for (const auto &read : txn.readset()) {
      if(!server->IsKeyOwned(read.key())) {
        continue;
      }
      locks[read.key()].state = rwLock::LOCKED_READ;
      locks[read.key()].owners.insert(digest);
    }

    for (const auto &write : txn.writeset()) {
      if(!server->IsKeyOwned(write.key())) {
        continue;
      }
      locks[write.key()].state = rwLock::LOCKED_WRITE;
      locks[write.key()].owners.insert(digest);
    }

    return true;
}

bool Augustus::ReleaseLock(const proto::Transaction& txn, const std::string &digest, class Server* server) {

    for (const auto &read : txn.readset()) {
      if(!server->IsKeyOwned(read.key())) {
        continue;
      }
      if (!locks.count(read.key()) ||
          !locks[read.key()].owners.count(digest)) {
          // Same explanation as for write lock below
          //Panic("Error on Augustus release lock: read");
          locks[read.key()] = rwLock();
      }
      locks[read.key()].owners.erase(digest);

      if (locks[read.key()].owners.size() == 0) {
          locks[read.key()].state = rwLock::FREE;
      }
    }

    for (const auto &write : txn.writeset()) {
      if(!server->IsKeyOwned(write.key())) {
        continue;
      }
      if (!locks.count(write.key()) ||
          !locks[write.key()].owners.count(digest)) {
          // GroupedDecision comes before the state machine tries to acquire the locks for the transaction, so that later, HandleTransaction will not call augustus.TryLock because the grouped decision has been recorded
          // Therefore, nothing needs to be done to the locks
          //Panic("Error on Augustus release lock: write");
          locks[write.key()] = rwLock();
      } else {
          locks[write.key()].owners.erase(digest);
          locks[write.key()].state = rwLock::FREE;
      }
    }

    return true;
}
}
