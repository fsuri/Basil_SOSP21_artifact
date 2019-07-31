#include "store/mortystore/client.h"

namespace mortystore {


Client::Client(const std::string configPath, int nShards, int closestReplica,
    Transport *transport) : nshards(nShards), transport(transport),
    lastReqId(0UL) {
  // Initialize all state here;
  client_id = 0;
  while (client_id == 0) {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    client_id = dis(gen);
  }
  t_id = (client_id / 10000) * 10000;

  sclient.reserve(nshards);

  Debug("Initializing Morty client with id [%lu] %lu", client_id, nshards);

  /* Start a client for each shard. */
  for (uint64_t i = 0; i < nshards; i++) {
    std::string shardConfigPath = configPath + std::to_string(i) + ".config";
    sclient[i] = new ShardClient(shardConfigPath, transport,
        client_id, i, closestReplica);
  }

  Debug("Morty client [%lu] created! %lu %lu", client_id, nshards,
      sclient.size());
}

Client::~Client() {
  for (auto b : sclient) {
    delete b;
  }
}

void Client::Execute(AsyncTransaction *txn, execute_callback ecb) {
}

void Client::ExecuteNextOperation(AsyncTransaction *txn, Branch *branch) {
Operation op = currTxn->GetNextOperation(branch->opCount, branch->readValues);
  switch (op.type) {
    case GET: {
      Get(branch, op.key);
      break;
    }
    case PUT: {
      Put(branch, op.key, op.value);
      break;
    }
    case COMMIT: {
      Commit(branch);
      break;
    }
    case ABORT: {
      Abort(branch);
      currEcb(false, std::map<std::string, std::string>());
      break;
    }
    default:
      NOT_REACHABLE();
  }
}

void Client::Get(Branch *branch, const std::string &key) {
  Debug("GET [%lu : %lu : %s]", t_id, branch->id, key.c_str());

  // Contact the appropriate shard to get the value.
  int i = 0;//::Client::key_to_shard(key, nshards);
  if (branch->participants.find(i) == branch->participants.end()) {
    branch->participants.insert(i);
    //sclient[i]->Begin(t_id);
  }
  
  uint32_t timeout = 10000;
  sclient[i]->Get(t_id, key, std::bind(&mortystore::Client::GetCallback, this,
        branch, std::placeholders::_1, std::placeholders::_2,
        std::placeholders::_3),
      std::bind(&mortystore::Client::GetTimeout, this, branch,
        std::placeholders::_1, std::placeholders::_2), timeout);
}

void Client::Put(Branch *branch, const std::string &key,
    const std::string &value) {
  Debug("GET [%lu : %lu : %s, %s]", t_id, branch->id, key.c_str(),
      value.c_str());

  // Contact the appropriate shard to get the value.
  int i = 0;//::Client::key_to_shard(key, nshards);
  if (branch->participants.find(i) == branch->participants.end()) {
    branch->participants.insert(i);
    //sclient[i]->Begin(t_id);
  }
  
  uint32_t timeout = 10000;
  sclient[i]->Put(t_id, key, value, std::bind(&mortystore::Client::PutCallback,
        this, branch, std::placeholders::_1, std::placeholders::_2,
        std::placeholders::_3),
      std::bind(&mortystore::Client::PutTimeout, this, branch,
        std::placeholders::_1, std::placeholders::_2,
        std::placeholders::_3), timeout);
}

void Client::Commit(Branch *branch) {
  Debug("COMMIT [%lu : %lu]", t_id, branch->id);
  Panic("Not implemented.");
}

void Client::Abort(Branch *branch) {
  Debug("ABORT [%lu : %lu]", t_id, branch->id);
  Panic("Not implemented.");
}

void Client::GetCallback(Branch *branch, int status, const std::string &key,
    const std::string &val) {
}

void Client::PutCallback(Branch *branch, int status, const std::string &key,
    const std::string &val) {
}

void Client::GetTimeout(Branch *branch, int status, const std::string &key) {
}

void Client::PutTimeout(Branch *branch, int status, const std::string &key,
    const std::string &value) {
}

} // namespace mortystore
