#include "store/pbftstore/server.h"
#include "store/pbftstore/common.h"
#include "store/common/transaction.h"
#include <iostream>

namespace pbftstore {

using namespace std;

Server::Server(const transport::Configuration& config, KeyManager *keyManager, int groupIdx, int myId, int numShards, int numGroups, bool signMessages, bool validateProofs, uint64_t timeDelta, partitioner part, TrueTime timeServer) :
config(config), keyManager(keyManager), groupIdx(groupIdx), myId(myId), numShards(numShards), numGroups(numGroups), signMessages(signMessages), validateProofs(validateProofs),  timeDelta(timeDelta), part(part), timeServer(timeServer){

}

Server::~Server() {}

bool Server::CCC(const proto::Transaction& txn) {
  Timestamp txTs(txn.timestamp());
  // TODO actually do OCC check and add to prepared list
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
        return false;
      }
    }

    // check the prepared writes
    for (const auto& pair : pendingTransactions) {
      for (const auto& write : pair.second.writeset()) {
        if (write.key() == read.key()) {
          Timestamp wts(pair.second.timestamp());
          if (wts > rts && wts < txTs) {
            return false;
          }
        }
      }
    }
  }

  for (const auto &write : txn.writeset()) {
    if(!IsKeyOwned(write.key())) {
      continue;
    }

    // we want to make sure that no prepared/committed read spans
    // our writes

    // check commited reads
    // get a pointer to the first read that commits after this tx
    auto it = committedReads[write.key()].lower_bound(txTs);
    it++;
    // all iterator pairs committed after txTs (commit ts > txTs)
    // so we just need to check if they returned a version before txTs (read ts < txTs)
    while(it != committedReads[write.key()].end()) {
      if ((*it).second < txTs) {
        return false;
      }
      it++;
    }

    // next, check the prepared tx's read sets
    for (const auto& pair : pendingTransactions) {
      for (const auto& read : pair.second.readset()) {
        if (read.key() == write.key()) {
          Timestamp pendingTxTs(pair.second.timestamp());
          Timestamp rts(read.readtime());
          if (txTs > rts && txTs < pendingTxTs) {
            return false;
          }
        }
      }
    }
  }
  return true;

}

::google::protobuf::Message* Server::Execute(const std::string& type, const std::string& msg, proto::CommitProof &&commitProof) {
  cout << "Execute: " << type << endl;

  proto::Transaction transaction;
  if (type == transaction.GetTypeName()) {
    transaction.ParseFromString(msg);

    proto::TransactionDecision* decision = new proto::TransactionDecision();
    std::string digest = TransactionDigest(transaction);
    decision->set_txn_digest(digest);
    decision->set_shard_id(groupIdx);
    // OCC check
    if (CCC(transaction)) {
      decision->set_status(REPLY_OK);
      commitProofs[digest] = make_shared<proto::CommitProof>(std::move(commitProof));
      pendingTransactions[digest] = transaction;
    } else {
      decision->set_status(REPLY_FAIL);
    }
    // Send decision to client
    return decision;
  }
  return nullptr;
}

::google::protobuf::Message* Server::HandleMessage(const std::string& type, const std::string& msg) {
  cout << "Handle" << type << endl;

  proto::Read read;
  proto::GroupedDecision gdecision;

  if (type == read.GetTypeName()) {
    read.ParseFromString(msg);

    Timestamp ts(read.timestamp());
    std::pair<Timestamp, ValueAndProof> result;
    bool exists = commitStore.get(read.key(), ts, result);

    proto::ReadReply* readReply = new proto::ReadReply();
    readReply->set_req_id(read.req_id());
    readReply->set_key(read.key());
    if (exists) {
      readReply->set_status(REPLY_OK);
      readReply->set_value(result.second.value);
      result.first.serialize(readReply->mutable_value_timestamp());
      if (validateProofs) {
        *readReply->mutable_commit_proof() = *result.second.commitProof;
      }
    } else {
      readReply->set_status(REPLY_FAIL);
    }

    if (signMessages) {
      proto::SignedMessage *signedMessage = new proto::SignedMessage();
      SignMessage(*readReply, keyManager->GetPrivateKey(myId), myId, *signedMessage);
      delete readReply;
      return signedMessage;
    } else {
      return readReply;
    }
  } else if (type == gdecision.GetTypeName()) {
    gdecision.ParseFromString(msg);

    proto::GroupedDecisionAck* groupedDecisionAck = new proto::GroupedDecisionAck();

    std::string digest = gdecision.txn_digest();
    groupedDecisionAck->set_txn_digest(digest);
    if (gdecision.status() == REPLY_OK) {
      // verify gdecision
      if (verifyGDecision(gdecision) && pendingTransactions.find(digest) != pendingTransactions.end()) {
        proto::Transaction txn = pendingTransactions[digest];
        Timestamp ts(txn.timestamp());
        // apply tx
        for (const auto &read : txn.readset()) {
          if(!IsKeyOwned(read.key())) {
            continue;
          }
          committedReads[read.key()][ts] = read.readtime();
        }

        ValueAndProof valProof;
        std::shared_ptr<proto::CommitProof> commitProofPtr = commitProofs[digest];
        for (const auto &write : txn.writeset()) {
          if(!IsKeyOwned(write.key())) {
            continue;
          }

          valProof.value = write.value();
          valProof.commitProof = commitProofPtr;
          commitStore.put(write.key(), valProof, ts);

          // GC stuff?
          // auto rtsItr = rts.find(write.key());
          // if (rtsItr != rts.end()) {
          //   auto itr = rtsItr->second.begin();
          //   auto endItr = rtsItr->second.upper_bound(ts);
          //   while (itr != endItr) {
          //     itr = rtsItr->second.erase(itr);
          //   }
          // }
        }
        // TODO maybe remove commit proof

        // mark txn as commited
        pendingTransactions.erase(digest);
        groupedDecisionAck->set_status(REPLY_OK);
      } else {
        groupedDecisionAck->set_status(REPLY_FAIL);
      }
    } else {
      // abort the tx
      if (pendingTransactions.find(digest) != pendingTransactions.end()) {
        pendingTransactions.erase(digest);
      }
      groupedDecisionAck->set_status(REPLY_FAIL);
    }

    if (signMessages) {
      proto::SignedMessage *signedMessage = new proto::SignedMessage();
      SignMessage(*groupedDecisionAck, keyManager->GetPrivateKey(myId), myId, *signedMessage);
      delete groupedDecisionAck;
      return signedMessage;
    } else {
      return groupedDecisionAck;
    }
  }

  return nullptr;
}

bool Server::verifyGDecision(const proto::GroupedDecision& gdecision) {
  std::string digest = gdecision.txn_digest();
  proto::Transaction txn = pendingTransactions[digest];

  // We will go through the grouped decisions and make sure that each
  // decision is valid. Then, we will mark the shard for those decisions
  // as valid. We return true if all participating shard decisions are valid

  // This will hold the remaining shards that we need to verify
  std::unordered_set<uint64_t> remaining_shards;
  for (auto id : txn.participating_shards()) {
    remaining_shards.insert(id);
  }

  if (signMessages) {
    for (auto& decisions : gdecision.signed_decisions().grouped_decisions()) {
      if (remaining_shards.find(decisions.first) != remaining_shards.end()) {
        std::unordered_set<uint64_t> valid_decisions;
        for (auto& decision : decisions.second.decisions()) {
          std::string data;
          std::string type;
          if (ValidateSignedMessage(decision, keyManager, data, type)) {
            proto::TransactionDecision txnDecision;
            if (type == txnDecision.GetTypeName()) {
              txnDecision.ParseFromString(data);
              if (txnDecision.status() == REPLY_OK && txnDecision.txn_digest() == digest && txnDecision.shard_id() == decisions.first) {
                valid_decisions.insert(valid_decisions.size());
              }
            }
          }
        }
        if (valid_decisions.size() >= (uint64_t) config.f + 1) {
          remaining_shards.erase(decisions.first);
        }
      }
    }
  } else {
    for (auto& decisions : gdecision.decisions().grouped_decisions()) {
      if (remaining_shards.find(decisions.first) != remaining_shards.end()) {
        std::unordered_set<uint64_t> valid_decisions;
        for (auto& decision : decisions.second.decisions()) {
          if (decision.status() == REPLY_OK && decision.txn_digest() == digest && decision.shard_id() == decisions.first) {
            valid_decisions.insert(valid_decisions.size());
          }
        }
        if (valid_decisions.size() >= (uint64_t) config.f + 1) {
          remaining_shards.erase(decisions.first);
        }
      }
    }
  }

  return remaining_shards.size() == 0;
}

void Server::Load(const std::string &key, const std::string &value,
    const Timestamp timestamp) {
      Panic("Unimplemented");
}

Stats &Server::GetStats() {
  return stats;
}

}
