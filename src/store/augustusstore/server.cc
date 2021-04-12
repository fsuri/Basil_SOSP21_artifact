#include "store/augustusstore/server.h"
#include "store/augustusstore/common.h"
#include "store/common/transaction.h"
#include <iostream>
#include <sys/time.h>

namespace augustusstore {

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
}

Server::~Server() {}

std::vector<::google::protobuf::Message*> Server::Execute(const string& type, const string& msg) {
  Debug("Execute: %s", type.c_str());

  proto::Transaction transaction;
  proto::GroupedDecision gdecision;
  if (type == transaction.GetTypeName()) {
      transaction.ParseFromString(msg);
      return HandleTransaction(transaction);
  } else {
      Panic("Augustus only uses prepare messages for consensus (multi-cast), GroupedDecisions go through HandleMessage");
  }

  std::vector<::google::protobuf::Message*> results;
  results.push_back(nullptr);
  return results;
}

::google::protobuf::Message* Server::HandleMessage(const string& type, const string& msg) {
  Debug("Handle %s", type.c_str());

  proto::GroupedDecision gdecision;

  if (type == gdecision.GetTypeName()) {
    // Augustus doesn't use consensus or reply to client for grouped decisions
    gdecision.ParseFromString(msg);
    if (gdecision.status() == REPLY_OK) {
      HandleGroupedCommitDecision(gdecision);
    } else {
      HandleGroupedAbortDecision(gdecision);
    }
    return nullptr;
  }
  else{
    Panic("Request not of type GroupedDecision");
  }

  return nullptr;
}


std::vector<::google::protobuf::Message*> Server::HandleTransaction(const proto::Transaction& transaction) {
  std::unique_lock lock(atomicMutex); //TODO: might be able to make it finer.

  std::vector<::google::protobuf::Message*> results;
  proto::TransactionDecision* decision = new proto::TransactionDecision();

  string digest = TransactionDigest(transaction);
  Debug("Handling transaction");
  DebugHash(digest);
  stats.Increment("handle_tx",1);
  decision->set_txn_digest(digest);
  decision->set_shard_id(groupIdx);

  pendingTransactions[digest] = transaction;
  
  // check whether there has been a group decision of commit/abort
  if (bufferedGDecs.find(digest) != bufferedGDecs.end()) {
    stats.Increment("used_buffered_gdec",1);
    Debug("found buffered gdecision");

    // apply tx writes if committed
    if(bufferedGDecs[digest].status() == REPLY_OK){
        for (const auto &write : transaction.writeset()) {
            if(!IsKeyOwned(write.key())) {
                continue;
            }
            augustus.store[write.key()] = write.value();
            stats.Increment("apply_augustus_tx_write",1);
        }
    }

    bufferedGDecs.erase(digest);
    return results;
  }

  // Augustus lock check
  if (augustus.TryLock(transaction, this)) {
    stats.Increment("augustus_lock_succeed",1);
    Debug("augustus lock succeeded");
    decision->set_status(REPLY_OK);
    pendingTransactions[digest] = transaction;

    for (const auto &read : transaction.readset()) {
      if(!IsKeyOwned(read.key())) {
        continue;
      }
      stats.Increment("apply_augustus_tx_read",1);
      
      // attach read result to the decision message back to client
      proto::ReadResult *readResult = decision->add_readset();
      readResult->set_key(read.key());
      if (augustus.store.count(read.key())) {
          readResult->set_value(augustus.store[read.key()]);
      } else {
          readResult->set_value("");
      }
    }
  } else {
    stats.Increment("augustus_lock_fail",1);
    Debug("augustus lock failed");
    decision->set_status(REPLY_FAIL);
    pendingTransactions[digest] = transaction;
  }

  results.push_back(decision);

  return results;
}


::google::protobuf::Message* Server::HandleGroupedCommitDecision(const proto::GroupedDecision& gdecision) {

  Debug("Handling Grouped commit Decision");
  string digest = gdecision.txn_digest();
  DebugHash(digest);

  // check whether there has been a group decision of commit/abort
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

  // verify gdecision
  if (verifyGDecision_parallel(gdecision, pendingTransactions[digest], keyManager, signMessages, config.f, tp)) {
  //if (verifyGDecision(gdecision, pendingTransactions[digest], keyManager, signMessages, config.f)) {
 //if(true){

    std::unique_lock lock(atomicMutex);
    stats.Increment("apply_tx",1);
    stats.Increment("augustus_committed_decision",1);
    proto::Transaction txn = pendingTransactions[digest];
    Timestamp ts(txn.timestamp());

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
  } else {
    stats.Increment("gdec_failed_valid",1);
  }

  return nullptr;
}


::google::protobuf::Message* Server::HandleGroupedAbortDecision(const proto::GroupedDecision& gdecision) {

    Debug("Handling Grouped abort Decision");
    string digest = gdecision.txn_digest();
    DebugHash(digest);

    // check whether there has been a group decision of commit/abort
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

    // validate the abort message
    if(validate_abort){
        if(!verifyG_Abort_Decision(gdecision, pendingTransactions[digest], keyManager, signMessages, config.f)){
            Debug("failed validation for abort decision");
            return nullptr;
        }
    }

    std::unique_lock lock(atomicMutex);
    // abort the tx
    stats.Increment("augustus_aborted_decision",1);
    cleanupPendingTx(digest);
    // there is a chance that this abort comes before we see the tx, so save the decision
    abortedTxs.insert(digest);

    return nullptr;
}

void Server::cleanupPendingTx(std::string digest) {
  if (pendingTransactions.find(digest) != pendingTransactions.end()) {
    proto::Transaction tx = pendingTransactions[digest];

    // release Augustus locks
    augustus.ReleaseLock(tx, this);

    pendingTransactions.erase(digest);
  }
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

bool Server::CCC2(const proto::Transaction& txn) {
    Panic("Augustus doesn't use OCC");
    return true;
}

bool Server::CCC(const proto::Transaction& txn) {
    Panic("Augustus doesn't use OCC");
    return true;
}

::google::protobuf::Message* Server::HandleRead(const proto::Read& read) {
    Panic("Augustus doesn't handle read like Tapir");
    return nullptr;
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

bool Augustus::TryLock(const proto::Transaction& txn, class Server* server) {
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
    string digest = TransactionDigest(txn);

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

bool Augustus::ReleaseLock(const proto::Transaction& txn, class Server* server) {
    string digest = TransactionDigest(txn);

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
