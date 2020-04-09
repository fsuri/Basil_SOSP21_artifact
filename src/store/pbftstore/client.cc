#include "store/pbftstore/client.h"

#include "store/pbftstore/common.h"

namespace pbftstore {

using namespace std;

Client::Client(transport::Configuration *config, int nShards, int nGroups,
    int closestReplica, Transport *transport, partitioner part, bool syncCommit,
    uint64_t readQuorumSize, bool signedMessages, bool validateProofs,
    KeyManager *keyManager, TrueTime timeServer) : config(config),
    nshards(nShards), ngroups(nGroups), transport(transport), part(part),
    syncCommit(syncCommit), signedMessages(signedMessages),
    validateProofs(validateProofs), keyManager(keyManager),
    timeServer(timeServer), lastReqId(0UL) {
  client_id = 0;
  while (client_id == 0) {
    random_device rd;
    mt19937_64 gen(rd());
    uniform_int_distribution<uint64_t> dis;
    client_id = dis(gen);
  }
  // Initialize all state here;
  client_seq_num = 0;

  bclient.reserve(nshards);

  Debug("Initializing Indicus client with id [%lu] %lu", client_id, nshards);

  /* Start a client for each shard. */
  for (uint64_t i = 0; i < ngroups; i++) {
    bclient[i] = new ShardClient(config, transport, client_id, i,
        closestReplica, signedMessages, validateProofs,
        keyManager, timeServer);
  }

  Debug("Indicus client [%lu] created! %lu %lu", client_id, nshards,
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
  client_seq_num++;
  Debug("BEGIN [%lu]", client_seq_num);

  txn = proto::Transaction();
  txn.set_client_id(client_id);
  txn.set_client_seq_num(client_seq_num);
  // Optimistically choose a read timestamp for all reads in this transaction
  txn.mutable_timestamp()->set_timestamp(timeServer.GetTime());
  txn.mutable_timestamp()->set_id(client_id);
}

void Client::Get(const std::string &key, get_callback gcb,
    get_timeout_callback gtcb, uint32_t timeout) {
  Debug("GET [%lu : %s]", client_seq_num, key.c_str());

  // Contact the appropriate shard to get the value.
  int i = part(key, nshards) % ngroups;

  // If needed, add this shard to set of participants and send BEGIN.
  if (!IsParticipant(i)) {
    txn.add_involved_groups(i);
    bclient[i]->Begin(client_seq_num);
  }

  read_callback rcb = [gcb, this](int status, const std::string &key,
      const std::string &val, const Timestamp &ts, const proto::Transaction &dep,
      bool hasDep) {
    if (Message_DebugEnabled(__FILE__)) {
      uint64_t intValue = 0;
      for (int i = 0; i < 4; ++i) {
        intValue = intValue | (static_cast<uint64_t>(val[i]) << ((3 - i) * 8));
      }
      Debug("GET CALLBACK [%lu : %s] Read value %lu.", client_seq_num, key.c_str(), intValue);
    }
    ReadMessage *read = txn.add_read_set();
    read->set_key(key);
    ts.serialize(read->mutable_readtime());
    if (hasDep) {
      *txn.add_deps() = dep;
    }
    gcb(status, key, val, ts);
  };
  read_timeout_callback rtcb = gtcb;

  // Send the GET operation to appropriate shard.
  bclient[i]->Get(client_seq_num, key, txn.timestamp(), readQuorumSize, rcb,
      rtcb, timeout);
}

void Client::Put(const std::string &key, const std::string &value,
    put_callback pcb, put_timeout_callback ptcb, uint32_t timeout) {
  if (Message_DebugEnabled(__FILE__)) {
    uint64_t intValue = 0;
    for (int i = 0; i < 4; ++i) {
      intValue = intValue | (static_cast<uint64_t>(value[i]) << ((3 - i) * 8));
    }
    Debug("PUT [%lu : %s : %lu]", client_seq_num, key.c_str(), intValue);
  }

  // Contact the appropriate shard to set the value.
  int i = part(key, nshards) % ngroups;

  // If needed, add this shard to set of participants and send BEGIN.
  if (!IsParticipant(i)) {
    txn.add_involved_groups(i);
    bclient[i]->Begin(client_seq_num);
  }

  WriteMessage *write = txn.add_write_set();
  write->set_key(key);
  write->set_value(value);

  // Buffering, so no need to wait.
  bclient[i]->Put(client_seq_num, key, value, pcb, ptcb, timeout);
}

void Client::Commit(commit_callback ccb, commit_timeout_callback ctcb,
    uint32_t timeout) {
  uint64_t reqId = lastReqId++;
  PendingRequest *req = new PendingRequest(reqId);
  pendingReqs[reqId] = req;
  req->ccb = ccb;
  req->ctcb = ctcb;
  req->prepareTimestamp = new Timestamp(timeServer.GetTime(), client_id);
  req->callbackInvoked = false;

  Phase1(req, timeout);
}
