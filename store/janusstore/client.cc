// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
#include "store/janusstore/client.h"

namespace janusstore {

  using namespace std;
  using namespace proto;

  Client::Client(const string configPath, int nShards, int closestReplica, Transport * transport): nshards(nShards), transport(transport), config(nullptr) {
    // initialize a random client ID
    client_id = 0;
    while (client_id == 0) {
      random_device rd;
      mt19937_64 gen(rd());
      uniform_int_distribution<uint64_t> dis;
      client_id = dis(gen);
    }

    // for now, it does not seem like we need txn_id or ballot
    // MSB = client_id, LSB = txn num
    next_txn_id = (client_id / 10000) * 10000;
    // MSB = client_id, LSB = ballot num
    ballot = (client_id / 10000) * 10000;

    bclient.reserve(nshards);
    // Debug("Initializing Janus client with id [%llu] %llu [closestReplica: %i]", client_id, nshards, closestReplica);

    std::ifstream configStream(configPath);
    if (configStream.fail()) {
      Panic("Unable to read configuration file: %s\n", configPath.c_str());
    }
    config = new transport::Configuration(configStream);

    /* Start a shardclient for each shard. */
    // TODO change this to a single config file lul
    for (int i = 0; i < this->nshards; i++) {
      ShardClient * shardclient = new ShardClient(config,
        transport, client_id, i, closestReplica);
      // we use shardclients instead of bufferclients here
      bclient[i] = shardclient;
    }

    // Debug("Janus client [%llu] created! %llu %lu", client_id, nshards, bclient.size());
  }

  Client::Client(transport::Configuration *config, int nShards, int closestReplica, Transport * transport): nshards(nShards), transport(transport), config(config) {
    // initialize a random client ID
    client_id = 0;
    while (client_id == 0) {
      random_device rd;
      mt19937_64 gen(rd());
      uniform_int_distribution<uint64_t> dis;
      client_id = dis(gen);
    }

    // for now, it does not seem like we need txn_id or ballot
    // MSB = client_id, LSB = txn num
    next_txn_id = (client_id / 10000) * 10000;
    // MSB = client_id, LSB = ballot num
    ballot = (client_id / 10000) * 10000;

    bclient.reserve(nshards);
    // Debug("Initializing Janus client with id [%llu] %llu [closestReplica: %i]", client_id, nshards, closestReplica);

    /* Start a shardclient for each shard. */
    // TODO change this to a single config file lul
    for (int i = 0; i < this->nshards; i++) {
      ShardClient * shardclient = new ShardClient(config,
        transport, client_id, i, closestReplica);
      // we use shardclients instead of bufferclients here
      bclient[i] = shardclient;
    }

    // Debug("Janus client [%llu] created! %llu %lu", client_id, nshards, bclient.size());
  }

  Client::~Client() {
    // TODO delete the maps too?
    for (auto b: bclient) {
      delete b;
    }
    delete config;
  }

  uint64_t Client::keyToShard(string key, uint64_t nshards) {
    // default partition function from store/common/partitioner.cc
    uint64_t hash = 5381;
    const char * str = key.c_str();
    for (unsigned int i = 0; i < key.length(); i++) {
      hash = ((hash << 5) + hash) + (uint64_t) str[i];
    }
    return (hash % nshards);
  }

  void Client::setParticipants(Transaction * txn) {
    PendingRequest* req = this->pendingReqs[txn->getTransactionId()];
    req->participant_shards.clear();

    for (const auto & key: txn->read_set) {
      int i = this->keyToShard(key, nshards);
      if (req->participant_shards.find(i) == req->participant_shards.end()) {
        // Debug("txn %llu -> shard %i, key %s", txn->getTransactionId(), i, key.c_str());
        req->participant_shards.insert(i);
      }
      txn->groups.insert(i);
      txn->addShardedReadSet(key, i);
    }

    for (const auto & pair: txn->write_set) {
      int i = this->keyToShard(pair.first, nshards);
      // Debug("%i, %i", txn->getTransactionId(), i);
      if (req->participant_shards.find(i) == req->participant_shards.end()) {
        // Debug("txn %llu -> shard %i, key %s", txn->getTransactionId(), i, pair.first.c_str());
        req->participant_shards.insert(i);
      }
      txn->groups.insert(i);
      txn->addShardedWriteSet(pair.first, pair.second, i);
    }
  }

  void Client::Execute(OneShotTransaction *txn, execute_callback ecb) {
    Transaction t(this->next_txn_id);
    t.setTransactionStatus(proto::TransactionMessage::PREACCEPT);
    for (auto key : txn->GetReadSet()) {
      t.addReadSet(key);
    }
    for (auto kv : txn->GetWriteSet()) {
      t.addWriteSet(kv.first, kv.second);
    }
    this->next_txn_id++;
    PreAccept(&t, 0UL, ecb);
  }

  void Client::PreAccept(Transaction * txn, uint64_t ballot, execute_callback ecb) {

    uint64_t txn_id = txn->getTransactionId();

    PendingRequest *req = new PendingRequest(txn_id, ecb);
    pendingReqs[txn_id] = req;

    // Debug("%s\n", ("CLIENT - PREACCEPT - txn " + to_string(txn_id)).c_str());
    setParticipants(txn);

    for (auto p: req->participant_shards) {
      auto pcb = std::bind(&Client::PreAcceptCallback, this,
        txn_id, placeholders::_1, placeholders::_2);

      bclient[p]->PreAccept(*txn, ballot, pcb);
    }
  }

  void Client::Accept(uint64_t txn_id, set <uint64_t> deps, uint64_t ballot) {

    Debug("%s\n", ("CLIENT - ACCEPT - txn " + to_string(txn_id)).c_str());

    PendingRequest* req = this->pendingReqs[txn_id];

    for (auto p : req->participant_shards) {
      std::vector<uint64_t> vec_deps(deps.begin(), deps.end());
      auto acb = std::bind(&Client::AcceptCallback, this, txn_id, placeholders::_1, placeholders::_2);

      bclient[p]->Accept(txn_id, vec_deps, ballot, acb);
    }
  }

  void Client::Commit(uint64_t txn_id, set<uint64_t> deps) {
    // Debug("%s\n", ("CLIENT - COMMIT - txn " + to_string(txn_id)).c_str());

    PendingRequest* req = this->pendingReqs[txn_id];

    // TODO supply each shardclient with the aggregated_depmeta
    // so that when committing, every replica knows which shards to talk to
    // for any dependency

    for (auto p : req->participant_shards) {
      std::vector<uint64_t> vec_deps(deps.begin(), deps.end());
      auto ccb = std::bind(&Client::CommitCallback, this, txn_id, placeholders::_1, placeholders::_2);
      bclient[p]->Commit(txn_id, vec_deps, req->aggregated_depmeta, ccb);
    }
  }

  void Client::PreAcceptCallback(uint64_t txn_id, int shard, std::vector<janusstore::proto::Reply> replies) {

    /* shardclient invokes this when all replicas in a shard have responded */
    // Debug("%s\n", ("CLIENT - PREACCEPT CB - txn " + to_string(txn_id) + " - shard - " + to_string(shard)).c_str());

    PendingRequest* req = this->pendingReqs[txn_id];

    // if we have not heard from this shard, we assume a fast quorum
    // until we process the replies
    if (req->responded_shards.find(shard) == req->responded_shards.end()) {
      req->has_fast_quorum = true;
    }

    // update responded shards
    req->responded_shards.insert(shard);

    // check if each replica within shard has the same dependencies
    // then aggregate dependencies
    bool fast_quorum = true;

    // aggregated replica deps for this shard
    std::set<uint64_t> shard_deps;

    UW_ASSERT(replies.size() != 0);

    for (auto reply: replies) {
      if (reply.op() == Reply::PREACCEPT_OK) {
        // parse message for deps
        DependencyList msg = reply.preaccept_ok().dep();
        for (int i = 0; i < msg.txnid_size(); i++) {
          uint64_t dep_id = msg.txnid(i);

          // add dep to aggregated set
          req->aggregated_deps.insert(dep_id);
          // add to deplist for this shard
          shard_deps.insert(dep_id);
        }

        // parse message for depmeta; will eventually replace DependencyList
        /*
         * Note: each replica is telling us both the dependency txns and their
         * participating groups, which we will aggregate for all dependencies
         * so that during COMMIT, each replica will know which shard to talk to
         * if they need to inquire.
         *
         * god I wish the original Janus paper mentioned this.
         */
        for (int i = 0; i < reply.preaccept_ok().depmeta_size(); i++) {
          DependencyMeta depmeta = reply.preaccept_ok().depmeta(i);
          for (int j = 0; j < depmeta.group_size(); j++) {
            req->aggregated_depmeta[depmeta.txnid()].insert(depmeta.group(j));
          }
        }

      } else {
        // meaning we will need to go to Accept phase
        fast_quorum = false;
      }
    }

    fast_quorum = fast_quorum && (req->aggregated_deps == shard_deps);
    req->has_fast_quorum = req->has_fast_quorum && fast_quorum;

    if (req->responded_shards.size() == req->participant_shards.size()) {
      req->responded_shards.clear();
      if (req->has_fast_quorum) {
        Commit(txn_id, req->aggregated_deps);
      } else {
        this->ballot++;
        Accept(txn_id, req->aggregated_deps, this->ballot);
      }
    }
  }

  void Client::AcceptCallback(uint64_t txn_id, int shard, std::vector<janusstore::proto::Reply> replies) {

    /* shardclient invokes this when all replicas in a shard have responded */
    // Debug("%s\n", ("CLIENT - ACCEPT CB - txn " + to_string(txn_id) + " - shard - " + to_string(shard)).c_str());

    PendingRequest* req = this->pendingReqs[txn_id];

    req->responded_shards.insert(shard);

    for (auto reply: replies) {
      if (reply.op() == Reply::ACCEPT_NOT_OK) {
        // if majority not okay, then goto failure recovery (not supported)
      }
    }

    if (req->responded_shards.size() == req->participant_shards.size()) {
      // no need to check for a quorum for every shard because we dont implement failure recovery
      Debug("CLIENT - AcceptCallback - proceeding to Commit phase");
      req->responded_shards.clear();
      Commit(txn_id, req->aggregated_deps);
    }
    return;
  }
  void Client::CommitCallback(uint64_t txn_id, int shard, std::vector<janusstore::proto::Reply> replies) {

    /* shardclient invokes this when all replicas in a shard have responded */
    // Debug("%s\n", ("CLIENT - COMMIT CB - txn " + to_string(txn_id) + " - shard - " + to_string(shard)).c_str());

    PendingRequest* req = this->pendingReqs[txn_id];

    req->responded_shards.insert(shard);
    Debug("CLIENT - CommitCallback - received %llu out of %llu shard responses", req->responded_shards.size(), req->participant_shards.size());

    // printf("%s\n", ("CLIENT - COMMIT CB - added " + to_string(shard) + " to responded list").c_str());

    if (req->responded_shards.size() == req->participant_shards.size()) {
      // return results to async_transaction_bench_client by invoking output commit callback
      Debug("Invoking execute callback for txn %llu", txn_id);
      stats.Increment("commits", 1);
      req->ccb(0, std::map<std::string, std::string>());
      req->responded_shards.clear();
    } else {
      // printf("%s\n", ("CLIENT - COMMIT CB - " + to_string(req->responded_shards.size()) + " shards responded out of " + to_string(req->participant_shards.size())).c_str());
    }
    return;
  }

  void Client::Read(string key, read_callback rcb) {
    Debug("%s\n", ("CLIENT - READ - key" + key).c_str());

    this->readReqs[key] = rcb;
    int shard = this->keyToShard(key, nshards);

    auto pcb = std::bind(&Client::ReadCallback, this,
      placeholders::_1, placeholders::_2);

    bclient[shard]->Read(key, pcb);
  }

  void Client::ReadCallback(string key, string value) {
    QNotice("GOT: <%s, %s>", key.c_str(), value.c_str());
    this->readReqs[key]();
  }

}
