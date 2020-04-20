#include "store/pbftstore/client.h"

#include "store/pbftstore/common.h"

namespace pbftstore {

using namespace std;

Client::Client(const transport::Configuration& config, int nGroups, int nShards,
      Transport *transport, partitioner part,
      uint64_t readQuorumSize, bool signMessages,
      bool validateProofs, KeyManager *keyManager,
      TrueTime timeserver) : config(config), nshards(nShards),
    ngroups(nGroups), transport(transport), part(part), readQuorumSize(readQuorumSize),
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

  Debug("Initializing PBFT client with id [%lu] %lu", client_id, ngroups);

  /* Start a client for each shard. */
  for (uint64_t i = 0; i < ngroups; i++) {
    bclient[i] = new ShardClient(config, transport, i,
        signMessages, validateProofs, keyManager);
  }

  Debug("PBFT client [%lu] created! %lu %lu", client_id, ngroups,
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

  currentTxn = proto::Transaction();
  // Optimistically choose a read timestamp for all reads in this transaction
  currentTxn.mutable_timestamp()->set_timestamp(timeServer.GetTime());
  currentTxn.mutable_timestamp()->set_id(client_id);
}

void Client::Get(const std::string &key, get_callback gcb,
    get_timeout_callback gtcb, uint32_t timeout) {
  Debug("GET [%s]", key.c_str());

  // Contact the appropriate shard to get the value.
  int i = part(key, nshards) % ngroups;

  // If needed, add this shard to set of participants and send BEGIN.
  if (!IsParticipant(i)) {
    currentTxn.add_participating_shards(i);
  }

  read_callback rcb = [gcb, this](int status, const std::string &key,
      const std::string &val, const Timestamp &ts) {
    if (status == REPLY_OK) {
      ReadMessage *read = currentTxn.add_readset();
      read->set_key(key);
      ts.serialize(read->mutable_readtime());
    }
    gcb(status, key, val, ts);
  };
  read_timeout_callback rtcb = gtcb;

  // Send the GET operation to appropriate shard.
  bclient[i]->Get(key, currentTxn.timestamp(), readQuorumSize, rcb,
      rtcb, timeout);
}

void Client::Put(const std::string &key, const std::string &value,
    put_callback pcb, put_timeout_callback ptcb, uint32_t timeout) {
  // Contact the appropriate shard to set the value.
  int i = part(key, nshards) % ngroups;

  // If needed, add this shard to set of participants and send BEGIN.
  if (!IsParticipant(i)) {
    currentTxn.add_participating_shards(i);
  }

  WriteMessage *write = currentTxn.add_writeset();
  write->set_key(key);
  write->set_value(value);
  // Buffering, so no need to wait.
  pcb(REPLY_OK, key, value);
}

void Client::Commit(commit_callback ccb, commit_timeout_callback ctcb,
    uint32_t timeout) {
  std::string digest = TransactionDigest(currentTxn);
  if (pendingPrepares.find(digest) == pendingPrepares.end()) {
    PendingPrepare pendingPrepare;
    pendingPrepare.ccb = ccb;
    pendingPrepare.ctcb = ctcb;
    pendingPrepare.timeout = timeout;
    // should do a copy
    pendingPrepare.txn = currentTxn;
    pendingPrepares[digest] = pendingPrepare;

    for (const auto& shard_id : currentTxn.participating_shards()) {

      prepare_timeout_callback pcbt = [](int s) {
        Debug("prepare timeout called");
      };
      if (signMessages) {
        signed_prepare_callback pcb = std::bind(&Client::HandleSignedPrepareReply,
          this, digest, shard_id, std::placeholders::_1, std::placeholders::_2);

        bclient[shard_id]->SignedPrepare(currentTxn, pcb, pcbt, timeout);
      } else {
        prepare_callback pcb = std::bind(&Client::HandlePrepareReply,
          this, digest, shard_id, std::placeholders::_1, std::placeholders::_2);

        bclient[shard_id]->Prepare(currentTxn, pcb, pcbt, timeout);
      }
    }
  }

}

void Client::HandleSignedPrepareReply(std::string digest, uint64_t shard_id, int status, const proto::GroupedSignedMessage& gsm) {
  if (pendingPrepares.find(digest) != pendingPrepares.end()) {
    PendingPrepare* pp = &pendingPrepares[digest];

    if (pp->signedShardDecisions.find(shard_id) == pp->signedShardDecisions.end()) {
      pp->signedShardDecisions[shard_id] = gsm;
      // abort on even a single shard abort
      if (status != REPLY_OK) {
        // TODO send abort writeback to all shards
        commit_callback ccb = pp->ccb;
        pendingPrepares.erase(digest);
        ccb(REPLY_FAIL);
        return;
      }

      if (pp->signedShardDecisions.size() == (uint64_t) pp->txn.participating_shards_size()) {
        proto::ShardSignedDecisions dec;
        for (const auto& pair : pp->signedShardDecisions) {
          (*dec.mutable_grouped_decisions())[pair.first] = pair.second;
        }
        commit_callback ccb = pp->ccb;
        commit_timeout_callback ctcb = pp->ctcb;
        uint32_t timeout = pp->timeout;
        proto::Transaction txn = pp->txn;
        pendingPrepares.erase(digest);
        this->WriteBackSigned(dec, txn, ccb, ctcb, timeout);
      }
    }
  }
}

void Client::HandlePrepareReply(std::string digest, uint64_t shard_id, int status, const proto::TransactionDecision& txndec) {
  if (pendingPrepares.find(digest) != pendingPrepares.end()) {
    PendingPrepare* pp = &pendingPrepares[digest];

    // make sure we haven't marked this shard's decision yet
    if (pp->shardDecisions.find(shard_id) == pp->shardDecisions.end()) {
      // add this shard to the list of replies
      pp->shardDecisions[shard_id] = txndec;

      // if we got an abort, tx no longer in progress
      if (status != REPLY_OK) {
        // TODO send abort writeback to all shards
        commit_callback ccb = pp->ccb;
        pendingPrepares.erase(digest);
        ccb(REPLY_FAIL);
        return;
      }

      // wait for all callbacks to complete
      if (pp->shardDecisions.size() == (uint64_t) pp->txn.participating_shards_size()) {
        proto::ShardDecisions dec;
        for (const auto& pair : pp->shardDecisions) {
          (*dec.mutable_grouped_decisions())[pair.first] = pair.second;
        }
        commit_callback ccb = pp->ccb;
        commit_timeout_callback ctcb = pp->ctcb;
        uint32_t timeout = pp->timeout;
        proto::Transaction txn = pp->txn;
        pendingPrepares.erase(digest);
        this->WriteBack(dec, txn, ccb, ctcb, timeout);
      }
    }
  }
}

void Client::WriteBackSigned(const proto::ShardSignedDecisions& dec, const proto::Transaction& txn,
  commit_callback ccb, commit_timeout_callback ctcb, uint32_t timeout) {
  std::string digest = TransactionDigest(txn);
  if (pendingWritebacks.find(digest) == pendingWritebacks.end()) {
    PendingWriteback pendingWriteback;
    pendingWriteback.ccb = ccb;
    pendingWriteback.txn = txn;
    pendingWritebacks[digest] = pendingWriteback;

    for (const auto& shard_id : txn.participating_shards()) {
      writeback_callback wcb = std::bind(&Client::HandleWritebackReply,
        this, digest, shard_id);

      writeback_timeout_callback wcbt = [](int s) {
          Debug("timeout called");
      };

      bclient[shard_id]->CommitSigned(digest, dec, wcb, wcbt, timeout);
    }
  }
}

void Client::WriteBack(const proto::ShardDecisions& dec, const proto::Transaction& txn,
  commit_callback ccb, commit_timeout_callback ctcb, uint32_t timeout) {
  std::string digest = TransactionDigest(txn);
  if (pendingWritebacks.find(digest) == pendingWritebacks.end()) {
    PendingWriteback pendingWriteback;
    pendingWriteback.ccb = ccb;
    pendingWriteback.txn = txn;
    pendingWritebacks[digest] = pendingWriteback;

    for (const auto& shard_id : txn.participating_shards()) {
      writeback_callback wcb = std::bind(&Client::HandleWritebackReply,
        this, digest, shard_id);

      writeback_timeout_callback wcbt = [](int s) {
          Debug("timeout called");
      };

      bclient[shard_id]->Commit(digest, dec, wcb, wcbt, timeout);
    }
  }
}

void Client::HandleWritebackReply(std::string digest, uint64_t shard_id) {
  if (pendingWritebacks.find(digest) != pendingWritebacks.end()) {
    PendingWriteback* pw = &pendingWritebacks[digest];
    pw->writebackAcks.insert(shard_id);
    if (pw->writebackAcks.size() == (uint64_t) pw->txn.participating_shards_size()) {
      commit_callback ccb = pw->ccb;
      pendingWritebacks.erase(digest);
      ccb(REPLY_OK);
    }
  }
}

void Client::Abort(abort_callback acb, abort_timeout_callback atcb,
    uint32_t timeout) {
  Panic("Unimplemented");
  // immediately invoke callback
  acb();
}

vector<int> Client::Stats() {
    vector<int> v;
    return v;
}

bool Client::IsParticipant(int g) {
  for (const auto &participant : currentTxn.participating_shards()) {
    if (participant == (uint64_t) g) {
      return true;
    }
  }
  return false;
}


}
