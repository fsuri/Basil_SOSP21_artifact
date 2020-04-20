#include "store/pbftstore/server.h"
#include "store/pbftstore/common.h"
#include "store/common/transaction.h"
#include <iostream>

namespace pbftstore {

using namespace std;

Server::Server(const transport::Configuration& config, KeyManager *keyManager,
  int groupIdx, int idx, int numShards, int numGroups, bool signMessages,
  bool validateProofs, uint64_t timeDelta, partitioner part,
  TrueTime timeServer) : config(config), keyManager(keyManager),
  groupIdx(groupIdx), idx(idx), id(groupIdx * config.n + idx),
  numShards(numShards), numGroups(numGroups), signMessages(signMessages),
  validateProofs(validateProofs),  timeDelta(timeDelta), part(part),
  timeServer(timeServer) {}

Server::~Server() {}

bool Server::CCC(const proto::Transaction& txn) {
  Debug("Starting ccc check");
  Timestamp txTs(txn.timestamp());
  // TODO do the iterative version
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

::google::protobuf::Message* Server::Execute(const string& type, const string& msg) {
  Debug("Execute: %s", type.c_str());

  proto::Transaction transaction;
  if (type == transaction.GetTypeName()) {
    transaction.ParseFromString(msg);

    return HandleTransaction(transaction);
  }
  return nullptr;
}

::google::protobuf::Message* Server::HandleTransaction(const proto::Transaction& transaction) {
  proto::TransactionDecision* decision = new proto::TransactionDecision();
  string digest = TransactionDigest(transaction);
  decision->set_txn_digest(digest);
  decision->set_shard_id(groupIdx);
  // OCC check
  if (CCC(transaction)) {
    Debug("ccc succeeded");
    decision->set_status(REPLY_OK);
    pendingTransactions[digest] = transaction;
  } else {
    Debug("ccc failed");
    decision->set_status(REPLY_FAIL);
  }

  return returnMessage(decision);
}

::google::protobuf::Message* Server::HandleMessage(const string& type, const string& msg) {
  Debug("Handle %s", type.c_str());

  proto::Read read;
  proto::GroupedDecision gdecision;

  if (type == read.GetTypeName()) {
    read.ParseFromString(msg);

    return HandleRead(read);
  } else if (type == gdecision.GetTypeName()) {
    gdecision.ParseFromString(msg);

    return HandleGroupedDecision(gdecision);
  }

  return nullptr;
}

::google::protobuf::Message* Server::HandleRead(const proto::Read& read) {
  Timestamp ts(read.timestamp());
  pair<Timestamp, ValueAndProof> result;
  bool exists = commitStore.get(read.key(), ts, result);

  proto::ReadReply* readReply = new proto::ReadReply();
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
    Debug("Read does not exit for key: %s", read.key().c_str());
    readReply->set_status(REPLY_FAIL);
  }

  return returnMessage(readReply);
}

::google::protobuf::Message* Server::HandleGroupedDecision(const proto::GroupedDecision& gdecision) {
  proto::GroupedDecisionAck* groupedDecisionAck = new proto::GroupedDecisionAck();

  string digest = gdecision.txn_digest();
  groupedDecisionAck->set_txn_digest(digest);
  if (gdecision.status() == REPLY_OK) {
    // verify gdecision
    if (verifyGDecision(gdecision) && pendingTransactions.find(digest) != pendingTransactions.end()) {
      proto::Transaction txn = pendingTransactions[digest];
      Timestamp ts(txn.timestamp());
      // apply tx
      Debug("applying tx");
      for (const auto &read : txn.readset()) {
        if(!IsKeyOwned(read.key())) {
          continue;
        }
        Debug("applying read to key %s", read.key().c_str());
        committedReads[read.key()][ts] = read.readtime();
      }

      proto::CommitProof proof;
      *proof.mutable_writeback_message() = gdecision;
      *proof.mutable_txn() = txn;
      shared_ptr<proto::CommitProof> commitProofPtr = make_shared<proto::CommitProof>(move(proof));

      for (const auto &write : txn.writeset()) {
        if(!IsKeyOwned(write.key())) {
          continue;
        }

        ValueAndProof valProof;

        valProof.value = write.value();
        valProof.commitProof = commitProofPtr;
        Debug("applying write to key %s", write.key().c_str());
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
  Debug("decision ack status: %d", groupedDecisionAck->status());

  return returnMessage(groupedDecisionAck);
}

bool Server::verifyGDecision(const proto::GroupedDecision& gdecision) {
  string digest = gdecision.txn_digest();
  proto::Transaction txn = pendingTransactions[digest];

  // We will go through the grouped decisions and make sure that each
  // decision is valid. Then, we will mark the shard for those decisions
  // as valid. We return true if all participating shard decisions are valid

  // This will hold the remaining shards that we need to verify
  unordered_set<uint64_t> remaining_shards;
  for (auto id : txn.participating_shards()) {
    Debug("requiring %lu", id);
    remaining_shards.insert(id);
  }

  if (signMessages) {
    // iterate over all shards
    for (const auto& pair : gdecision.signed_decisions().grouped_decisions()) {
      uint64_t shard_id = pair.first;
      proto::GroupedSignedMessage grouped = pair.second;
      // check if we are still looking for a grouped decision from this shard
      if (remaining_shards.find(shard_id) != remaining_shards.end()) {
        // unpack the message in the grouped signed message
        proto::PackedMessage packedMsg;
        if (packedMsg.ParseFromString(grouped.packed_msg())) {
          // make sure the packed message is a Transaction Decision
          proto::TransactionDecision decision;
          if (decision.ParseFromString(packedMsg.msg())) {
            // verify that the transaction decision is valid
            if (decision.status() == REPLY_OK &&
                decision.txn_digest() == digest &&
                decision.shard_id() == shard_id) {
              proto::SignedMessage signedMsg;
              // use this to keep track of the replicas for whom we have gotten
              // a valid signature.
              unordered_set<uint64_t> valid_signatures;

              // now verify all of the signatures
              for (const auto& id_sig_pair: grouped.signatures()) {
                Debug("ungrouped transaction decision for %lu", id_sig_pair.first);
                // recreate the signed message for the given replica id
                signedMsg.set_replica_id(id_sig_pair.first);
                signedMsg.set_signature(id_sig_pair.second);

                if (CheckSignature(signedMsg, keyManager)) {
                  valid_signatures.insert(id_sig_pair.first);
                } else {
                  Debug("Failed to validate transaction decision signature for %lu", id_sig_pair.first);
                }
              }

              // If we have confirmed f+1 signatures, then we mark this shard
              // as verifying the decision
              if (valid_signatures.size() >= (uint64_t) config.f + 1) {
                Debug("signed: verified shard %lu", shard_id);
                remaining_shards.erase(shard_id);
              }
            }
          }
        }
      }
    }
  } else {
    for (const auto& pair : gdecision.decisions().grouped_decisions()) {
      uint64_t shard_id = pair.first;
      proto::TransactionDecision decision = pair.second;
      if (remaining_shards.find(shard_id) != remaining_shards.end()) {
        if (decision.status() == REPLY_OK &&
            decision.txn_digest() == digest &&
            decision.shard_id() == shard_id) {
          remaining_shards.erase(shard_id);
        }
      }
    }
  }

  // the grouped decision should have a proof for all of the participating shards
  return remaining_shards.size() == 0;
}

void Server::Load(const string &key, const string &value,
    const Timestamp timestamp) {
      Panic("Unimplemented");
}

Stats &Server::GetStats() {
  return stats;
}

}
