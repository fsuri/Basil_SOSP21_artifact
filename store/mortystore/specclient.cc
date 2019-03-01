#include "store/mortystore/specclient.h"

namespace mortystore {

SpecClient::SpecClient(const std::string configPath, int nShards,
    int closestReplica) : nshards(nShards), transport(0.0, 0.0, 0, false) {
  client_id = 0;
  while (client_id == 0) {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    client_id = dis(gen);
  }
  t_id = (client_id/10000)*10000;
  
  sclients.reserve(nshards);

  Debug("Initializing Morty client with id [%lu] %lu", client_id, nshards);

  for (uint64_t i = 0; i < nshards; ++i) {
    string shardConfigPath = configPath + std::to_string(i) + ".config";
    sclients[i] = new ShardClient(shardConfigPath, &transport, client_id, i,
        closestReplica);
  }
}

SpecClient::~SpecClient() {
  transport.Stop();
  for (auto m : sclients) {
    delete m;
  }
}

void SpecClient::Begin() {
  // make sure we clean up the previous transaction execution
  //   which could include several branches still waiting for
  //   responses from shards
}

void SpecClient::Get(const std::string &key, get_callback cb) {
  Debug("GET [%lu : %s]", t_id, key.c_str());

  // Contact the appropriate shard to get the value.
  int i = Client::key_to_shard(key, nshards);

  // If needed, add this shard to set of participants and send BEGIN.
  if (participants.find(i) == participants.end()) {
    participants.insert(i);
    sclients[i]->Begin(t_id);
  }

  sclients[i]->Get(t_id, key, cb);
}

void SpecClient::Put(const std::string &key, const std::string &value,
    put_callback cb) {
}

void SpecClient::Commit(commit_callback cb) {
}
  
void SpecClient::Abort(abort_callback cb) {
}

std::vector<int> SpecClient::Stats() {
  return std::vector<int>();
}

} // namespace mortystore
