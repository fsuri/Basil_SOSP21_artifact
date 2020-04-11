#include "store/pbftstore/client.h"

#include "store/pbftstore/common.h"

namespace pbftstore {

using namespace std;

Client::Client(const transport::Configuration& config, int nGroups, int nShards,
      Transport *transport, partitioner part,
      uint64_t readQuorumSize, bool signMessages,
      bool validateProofs, KeyManager *keyManager,
      TrueTime timeserver) : config(config), nshards(nShards),
    ngroups(nGroups), transport(transport), part(part),
    signMessages(signMessages),
    validateProofs(validateProofs), keyManager(keyManager),
    timeServer(timeserver) {
  // just an invariant for now for everything to work ok
  assert(nGroups == nShards);

  // generate a random client uuid
  client_id = 0;
  while (client_id == 0) {
    random_device rd;
    mt19937_64 gen(rd());
    uniform_int_distribution<uint64_t> dis;
    client_id = dis(gen);
  }

  bclient.reserve(ngroups);

  Debug("Initializing Indicus client with id [%lu] %lu", client_id, ngroups);

  /* Start a client for each shard. */
  for (uint64_t i = 0; i < ngroups; i++) {
    bclient[i] = new ShardClient(config, transport, i,
        signMessages, validateProofs, keyManager);
  }

  Debug("Indicus client [%lu] created! %lu %lu", client_id, ngroups,
      bclient.size());
}

Client::~Client()
{
    for (auto b : bclient) {
        delete b;
    }
}

/* Begins a transaction. All subsequent operations before a commit() or
 * abort() are part of this transaction.
 */
void Client::Begin() {
  Debug("BEGIN tx");

  txn = proto::Transaction();
  // Optimistically choose a read timestamp for all reads in this transaction
  txn.mutable_timestamp()->set_timestamp(timeServer.GetTime());
  txn.mutable_timestamp()->set_id(client_id);
  txnInProgress = true;
  groupedDecision.clear();
  groupedSignedDecision.clear();
  writebackAcks.clear();
}

void Client::Get(const std::string &key, get_callback gcb,
    get_timeout_callback gtcb, uint32_t timeout) {
  Debug("GET [%s]", key.c_str());

  // Contact the appropriate shard to get the value.
  int i = part(key, nshards) % ngroups;

  // If needed, add this shard to set of participants and send BEGIN.
  if (!IsParticipant(i)) {
    txn.add_participating_shards(i);
  }

  read_callback rcb = [gcb, this](int status, const std::string &key,
      const std::string &val, const Timestamp &ts) {
    if (status == REPLY_OK) {
      ReadMessage *read = txn.add_readset();
      read->set_key(key);
      ts.serialize(read->mutable_readtime());
    }
    gcb(status, key, val, ts);
  };
  read_timeout_callback rtcb = gtcb;

  // Send the GET operation to appropriate shard.
  bclient[i]->Get(key, txn.timestamp(), readQuorumSize, rcb,
      rtcb, timeout);
}

void Client::Put(const std::string &key, const std::string &value,
    put_callback pcb, put_timeout_callback ptcb, uint32_t timeout) {
  // Contact the appropriate shard to set the value.
  int i = part(key, nshards) % ngroups;

  // If needed, add this shard to set of participants and send BEGIN.
  if (!IsParticipant(i)) {
    txn.add_participating_shards(i);
  }

  WriteMessage *write = txn.add_writeset();
  write->set_key(key);
  write->set_value(value);
  // Buffering, so no need to wait.
  pcb(REPLY_OK, key, value);
}

void Client::Commit(commit_callback ccb, commit_timeout_callback ctcb,
    uint32_t timeout) {
  for (const auto& shard_id : txn.participating_shards()) {
    if (signMessages) {
      signed_prepare_callback pcb = [ccb, ctcb, timeout, shard_id, this](int status, const proto::GroupedSignedDecisions& gsd) {
        if (this->groupedSignedDecision.find(shard_id) != this->groupedSignedDecision.end()) {
          this->groupedSignedDecision[shard_id] = gsd;
          if (status != REPLY_OK) {
            this->txnInProgress = false;
          }
          if (this->groupedSignedDecision.size() == (uint64_t) this->txn.participating_shards_size()) {
            if (this->txnInProgress) {
              proto::ShardSignedDecisions dec;
              for (const auto& pair : this->groupedSignedDecision) {
                (*dec.mutable_grouped_decisions())[pair.first] = pair.second;
              }
              this->WriteBackSigned(dec, ccb, ctcb, timeout);
            } else {
              ccb(REPLY_FAIL);
            }
          }
        }
      };

      prepare_timeout_callback pcbt = [](int s) {
        Debug("timeout called");
      };

      bclient[shard_id]->SignedPrepare(txn, pcb, pcbt, timeout);
    } else {
      prepare_callback pcb = [ccb, ctcb, timeout, shard_id, this](int status, const proto::GroupedDecisions& gd) {
        // make sure we haven't marked this shard's decision yet
        if (this->groupedDecision.find(shard_id) != this->groupedDecision.end()) {
          // add this shard to the list of replies
          this->groupedDecision[shard_id] = gd;
          // if we got an abort, tx no longer in progress
          if (status != REPLY_OK) {
            this->txnInProgress = false;
          }
          // wait for all callbacks to complete
          if (this->groupedDecision.size() == (uint64_t) this->txn.participating_shards_size()) {
            // if we didn't abort, go to the writeback stage
            if (this->txnInProgress) {
              proto::ShardDecisions dec;
              for (const auto& pair : this->groupedDecision) {
                (*dec.mutable_grouped_decisions())[pair.first] = pair.second;
              }
              this->WriteBack(dec, ccb, ctcb, timeout);
            } else {
              // otherwise, report a failure
              ccb(REPLY_FAIL);
            }
          }
        }
      };

      prepare_timeout_callback pcbt = [](int s) {
        Debug("timeout called");
      };

      bclient[shard_id]->Prepare(txn, pcb, pcbt, timeout);
    }
  }
}

void Client::WriteBack(const proto::ShardDecisions& dec, commit_callback ccb, commit_timeout_callback ctcb,
    uint32_t timeout) {
  std::string txnDig = TransactionDigest(txn);
  for (const auto& shard_id : txn.participating_shards()) {
    writeback_callback wcb = [this, shard_id, ccb]() {
      this->writebackAcks.insert(shard_id);
      if (this->writebackAcks.size() == (uint64_t) this->txn.participating_shards_size()) {
        ccb(REPLY_OK);
      }
    };
    writeback_timeout_callback wcbt = [](int s) {
        Debug("timeout called");
    };

    bclient[shard_id]->Commit(txnDig, dec, wcb, wcbt, timeout);
  }
}

void Client::WriteBackSigned(const proto::ShardSignedDecisions& dec, commit_callback ccb, commit_timeout_callback ctcb,
    uint32_t timeout) {
  std::string txnDig = TransactionDigest(txn);
  for (const auto& shard_id : txn.participating_shards()) {
    writeback_callback wcb = [this, shard_id, ccb]() {
      this->writebackAcks.insert(shard_id);
      if (this->writebackAcks.size() == (uint64_t) this->txn.participating_shards_size()) {
        ccb(REPLY_OK);
      }
    };
    writeback_timeout_callback wcbt = [](int s) {
        Debug("timeout called");
    };

    bclient[shard_id]->CommitSigned(txnDig, dec, wcb, wcbt, timeout);
  }

}

void Client::Abort(abort_callback acb, abort_timeout_callback atcb,
    uint32_t timeout) {

      Panic("Implement");
}

vector<int> Client::Stats() {
    vector<int> v;
    return v;
}

bool Client::IsParticipant(int g) {
  for (const auto &participant : txn.participating_shards()) {
    if (participant == (uint64_t) g) {
      return true;
    }
  }
  return false;
}


}
