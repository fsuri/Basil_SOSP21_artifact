#include "store/pbftstore/client.h"

#include "store/pbftstore/common.h"

namespace pbftstore {

using namespace std;

Client::Client(const transport::Configuration& config, int nGroups, int nShards,
      Transport *transport, Partitioner *part,
      uint64_t readQuorumSize, bool signMessages,
      bool validateProofs, KeyManager *keyManager,
      bool order_commit, bool validate_abort,
      TrueTime timeserver) : config(config), nshards(nShards),
    ngroups(nGroups), transport(transport), part(part), readQuorumSize(readQuorumSize),
    signMessages(signMessages),
    validateProofs(validateProofs), keyManager(keyManager),
    order_commit(order_commit), validate_abort(validate_abort),
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
  client_seq_num = 0;

  bclient.reserve(ngroups);

  Debug("Initializing PBFT client with id [%lu] %lu", client_id, ngroups);

  /* Start a client for each shard. */
  for (uint64_t i = 0; i < ngroups; i++) {
    bclient[i] = new ShardClient(config, transport, i,
        signMessages, validateProofs, keyManager, &stats, order_commit, validate_abort);
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
void Client::Begin(begin_callback bcb, begin_timeout_callback btcb, uint32_t timeout, bool retry) {
  transport->Timer(0, [this, bcb, btcb, timeout]() {
    Debug("BEGIN tx");

    client_seq_num++;
    currentTxn = proto::Transaction();
    // Optimistically choose a read timestamp for all reads in this transaction
    currentTxn.mutable_timestamp()->set_timestamp(timeServer.GetTime());
    currentTxn.mutable_timestamp()->set_id(client_id);
    bcb(client_seq_num);
  });
}

void Client::Get(const std::string &key, get_callback gcb,
    get_timeout_callback gtcb, uint32_t timeout) {
  transport->Timer(0, [this, key, gcb, gtcb, timeout]() {
    Debug("GET [%s]", key.c_str());

    // Contact the appropriate shard to get the value.
    std::vector<int> txnGroups;
    int i = (*part)(key, nshards, -1, txnGroups) % ngroups;

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
  });
}

void Client::Put(const std::string &key, const std::string &value,
    put_callback pcb, put_timeout_callback ptcb, uint32_t timeout) {
  transport->Timer(0, [this, key, value, pcb, ptcb, timeout]() {
    // Contact the appropriate shard to set the value.
    std::vector<int> txnGroups;
    int i = (*part)(key, nshards, -1, txnGroups) % ngroups;

    // If needed, add this shard to set of participants and send BEGIN.
    if (!IsParticipant(i)) {
      currentTxn.add_participating_shards(i);
    }

    WriteMessage *write = currentTxn.add_writeset();
    write->set_key(key);
    write->set_value(value);
    // Buffering, so no need to wait.
    pcb(REPLY_OK, key, value);
  });
}

void Client::Commit(commit_callback ccb, commit_timeout_callback ctcb,
    uint32_t timeout) {
  transport->Timer(0, [this, ccb, ctcb, timeout]() {
    std::string digest = TransactionDigest(currentTxn);
    if (pendingPrepares.find(digest) == pendingPrepares.end()) {
      PendingPrepare pendingPrepare;
      pendingPrepare.ccb = ccb;
      pendingPrepare.ctcb = ctcb;
      pendingPrepare.timeout = timeout;
      // should do a copy
      pendingPrepare.txn = currentTxn;
      pendingPrepares[digest] = pendingPrepare;

      if (currentTxn.participating_shards_size() == 0) {
        fprintf(stderr, "0 participating shards\n");
      }
      stats.Increment("called_commit",1);

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
    } else {
      fprintf(stderr, "already committed\n");
    }
  });
}

void Client::HandleSignedPrepareReply(std::string digest, uint64_t shard_id, int status,
  const proto::GroupedSignedMessage& gsm) {
  if (pendingPrepares.find(digest) != pendingPrepares.end()) {
    PendingPrepare* pp = &pendingPrepares[digest];

    // if(status == REPLY_OK){
    //   std::cerr << "got commit shard decision from shard_id " << shard_id << std::endl;
    // }
    // else{
    //   std::cerr << "got abort shard decision from shard_id " << shard_id << std::endl;
    // }

    if (pp->signedShardDecisions.find(shard_id) == pp->signedShardDecisions.end()) {

      pp->signedShardDecisions[shard_id] = std::move(gsm);  //instead of copying, can I move. Or release?

      // abort on even a single shard abort
      if (status != REPLY_OK) {
        proto::Transaction txn = pp->txn;
        commit_callback ccb = pp->ccb;

        //std::cerr << "ABORTING " << std::endl;
        if(validate_abort){
          proto::ShardSignedDecisions dec;
          (*dec.mutable_grouped_decisions())[shard_id] = pp->signedShardDecisions[shard_id];
          AbortTxnSigned(dec, txn, digest);
        }
        else{
          AbortTxn(txn);
        }
        pendingPrepares.erase(digest);
        ccb(ABORTED_SYSTEM);

        return;
      }

      if (pp->signedShardDecisions.size() == (uint64_t) pp->txn.participating_shards_size()) {
        //std::cerr << "COMMITTING " << std::endl;
        proto::ShardSignedDecisions dec;
        for (const auto& pair : pp->signedShardDecisions) {
          (*dec.mutable_grouped_decisions())[pair.first] = pair.second;
        }
        proto::Transaction txn = pp->txn;
        commit_callback ccb = pp->ccb;
        commit_timeout_callback ctcb = pp->ctcb;
        uint32_t timeout = pp->timeout;
        pendingPrepares.erase(digest);
        ccb(COMMITTED);
        //TODO:: remove callbacks...

        //WriteBackSigned(dec, txn, digest);

        this->WriteBackSigned(dec, txn, [](transaction_status_t tx_stat) {
          if (tx_stat != COMMITTED) {
            Panic("Writeback confirmation failed");
          }
          Debug("Got confirmation of writeback");
        }, []() {
          Debug("writeback confirmation timed out");
        }, timeout);
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
        proto::Transaction txn = pp->txn;
        commit_callback ccb = pp->ccb;
        pendingPrepares.erase(digest);
        ccb(ABORTED_SYSTEM);
        AbortTxn(txn);
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
        ccb(COMMITTED);
        this->WriteBack(dec, txn, [](transaction_status_t tx_stat) {
          if (tx_stat != COMMITTED) {
            Panic("Writeback confirmation failed");
          }
          Debug("Got confirmation of writeback");
        }, []() {
          Debug("writeback confirmation timed out");
        }, timeout);
      }
    }
  }
}


void Client::WriteBackSigned(const proto::ShardSignedDecisions& dec, const proto::Transaction& txn, std::string digest) {

    for (const auto& shard_id : txn.participating_shards()) {
      bclient[shard_id]->CommitSigned(digest, dec);
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
        this, digest, shard_id, std::placeholders::_1);

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
        this, digest, shard_id, std::placeholders::_1);

      writeback_timeout_callback wcbt = [](int s) {
          Debug("timeout called");
      };

      bclient[shard_id]->Commit(digest, dec, wcb, wcbt, timeout);
    }
  }
}

void Client::HandleWritebackReply(std::string digest, uint64_t shard_id, int status) {
  if (pendingWritebacks.find(digest) != pendingWritebacks.end()) {
    PendingWriteback* pw = &pendingWritebacks[digest];
    if (status == REPLY_FAIL) {
      commit_callback ccb = pw->ccb;
      pendingWritebacks.erase(digest);
      ccb(ABORTED_SYSTEM);
    } else {
      pw->writebackAcks.insert(shard_id);
      if (pw->writebackAcks.size() == (uint64_t) pw->txn.participating_shards_size()) {
        commit_callback ccb = pw->ccb;
        pendingWritebacks.erase(digest);
        ccb(COMMITTED);
      }
    }
  }
}

void Client::Abort(abort_callback acb, abort_timeout_callback atcb,
    uint32_t timeout) {
  transport->Timer(0, [this, acb, atcb, timeout]() {
    AbortTxn(currentTxn);
    // immediately invoke callback
    acb();
  });
}

void Client::AbortTxn(const proto::Transaction& txn) {
  stats.Increment("abort", 1);
  std::string digest = TransactionDigest(txn);
  proto::ShardSignedDecisions dec;
  for (const auto& shard_id : txn.participating_shards()) {
    bclient[shard_id]->Abort(digest, dec);
  }
}

void Client::AbortTxnSigned(const proto::ShardSignedDecisions& dec, const proto::Transaction& txn, std::string& digest){
  stats.Increment("abort", 1);
  //std::string digest = TransactionDigest(txn);
  if (pendingWritebacks.find(digest) == pendingWritebacks.end()) {
    PendingWriteback pendingWriteback;
    // pendingWriteback.ccb = ccb;
    // pendingWriteback.txn = txn;
    pendingWritebacks[digest] = pendingWriteback;
    std::cerr << "Downcalling to Shard client to send out Abort" << '\n';
    for (const auto& shard_id : txn.participating_shards()) {
      bclient[shard_id]->Abort(digest, dec);
    }
  }

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
