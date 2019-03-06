#include "store/tapirstore/asyncclient.h"

namespace tapirstore {

using namespace std;

AsyncClient::AsyncClient(const string configPath, int nShards,
                int closestReplica, TrueTime timeServer)
    : nshards(nShards), transport(0.0, 0.0, 0, false), timeServer(timeServer) {
    // Initialize all state here;
    client_id = 0;
    while (client_id == 0) {
        random_device rd;
        mt19937_64 gen(rd());
        uniform_int_distribution<uint64_t> dis;
        client_id = dis(gen);
    }
    t_id = (client_id/10000)*10000;

    bclient.reserve(nshards);

    Debug("Initializing Tapir client with id [%lu] %lu", client_id, nshards);

    /* Start a client for each shard. */
    for (uint64_t i = 0; i < nshards; i++) {
        string shardConfigPath = configPath + to_string(i) + ".config";
        ShardClient *shardclient = new ShardClient(shardConfigPath,
                &transport, client_id, i, closestReplica);
        bclient[i] = new BufferClient(shardclient);
    }

    Debug("Tapir client [%lu] created! %lu %lu", client_id, nshards, bclient.size());

    /* Run the transport in a new thread. */
    clientTransport = new thread(&AsyncClient::run_client, this);

    Debug("Tapir client [%lu] created! %lu", client_id, bclient.size());
}

AsyncClient::~AsyncClient()
{
    transport.Stop();
    for (auto b : bclient) {
        delete b;
    }
    clientTransport->join();
}

/* Runs the transport event loop. */
void
AsyncClient::run_client()
{
    transport.Run();
}

/* Begins a transaction. All subsequent operations before a commit() or
 * abort() are part of this transaction.
 *
 * Return a TID for the transaction.
 */
void AsyncClient::Begin() {
  Debug("BEGIN [%lu]", t_id + 1);
  t_id++;
  participants.clear();
}

/* Returns the value corresponding to the supplied key. */
void AsyncClient::Get(const string &key, get_callback cb) {
    Debug("GET [%lu : %s]", t_id, key.c_str());

    // Contact the appropriate shard to get the value.
    int i = Client::key_to_shard(key, nshards);

    // If needed, add this shard to set of participants and send BEGIN.
    if (participants.find(i) == participants.end()) {
        participants.insert(i);
        bclient[i]->Begin(t_id);
    }

    // Send the GET operation to appropriate shard.
    //Promise promise(GET_TIMEOUT, [&](Promise *p) {
    //  cb(p->GetReply(), key, p->GetValue());
    //});

    //bclient[i]->Get(key, &promise);
}

/* Sets the value corresponding to the supplied key. */
void AsyncClient::Put(const string &key, const string &value, put_callback cb) {
    Debug("PUT [%lu : %s]", t_id, key.c_str());

    // Contact the appropriate shard to set the value.
    int i = Client::key_to_shard(key, nshards);

    // If needed, add this shard to set of participants and send BEGIN.
    if (participants.find(i) == participants.end()) {
        participants.insert(i);
        bclient[i]->Begin(t_id);
    }

    //Promise promise(PUT_TIMEOUT, [&](Promise *p) {
    //  cb(p->GetReply(), key);  
    //});

    // Buffering, so no need to wait.
    //bclient[i]->Put(key, value, &promise);
}

void AsyncClient::Prepare(Timestamp &timestamp, commit_callback cb) {
  for (auto p : participants) {
    //preparePromises.push_back(new Promise(PREPARE_TIMEOUT,
    //    bind(&AsyncClient::PrepareCallback, this, cb, placeholders::_1
    //      )));
    bclient[p]->Prepare(timestamp, preparePromises.back());
  }
}

void AsyncClient::PrepareCallback(commit_callback cb, Promise *p) {
  uint64_t proposed = p->GetTimestamp().getTimestamp();

  --outstandingPrepares;
  switch(p->GetReply()) {
    case REPLY_OK:
      Debug("PREPARE [%lu] OK", t_id);
      break;
    case REPLY_FAIL:
      // abort!
      Debug("PREPARE [%lu] ABORT", t_id);
      prepareStatus = REPLY_FAIL;
      outstandingPrepares = 0;
      break;
    case REPLY_RETRY:
      prepareStatus = REPLY_RETRY;
      if (proposed > maxRepliedTs) {
        maxRepliedTs = proposed;
      }
      break;
    case REPLY_TIMEOUT:
      prepareStatus = REPLY_RETRY;
      break;
    case REPLY_ABSTAIN:
      // just ignore abstains
      break;
    default:
      break;
  }

  if (outstandingPrepares == 0) {
    switch (prepareStatus) {
      case REPLY_OK:
        Debug("COMMIT [%lu]", t_id);
        for (auto p : participants) {
            bclient[p]->Commit(0);
        }
        cb(true);
        break;
      case REPLY_RETRY:
        ++commitTries;
        if (commitTries < COMMIT_RETRIES) {
          uint64_t now = timeServer.GetTime();
          if (now > proposed) {
            prepareTimestamp->setTimestamp(now);
          } else {
            prepareTimestamp->setTimestamp(proposed);
          }
          Debug("RETRY [%lu] at [%lu]", t_id, prepareTimestamp->getTimestamp());
          Prepare(*prepareTimestamp, cb);
          break;
        }
      default:
        abort_callback acb;
        Abort(acb);
        cb(false);
        break;
    }
  }
}

void AsyncClient::ResetPrepare() {
  commitTries = 0;
  outstandingPrepares = 0;
  prepareStatus = REPLY_OK;
  maxRepliedTs = 0;
  for (auto p : this->preparePromises) {
    delete p;
  }
  preparePromises.clear();
}

/* Attempts to commit the ongoing transaction. */
void AsyncClient::Commit(commit_callback cb) {
  // TODO: this codepath is sketchy and probably has a bug (especially in the
  // failure cases)
  ResetPrepare();

  // Implementing 2 Phase Commit
  if (prepareTimestamp != NULL) {
    delete prepareTimestamp;
  }
  prepareTimestamp = new Timestamp(timeServer.GetTime(), client_id);
  Prepare(*prepareTimestamp, cb);
}

/* Aborts the ongoing transaction. */
void AsyncClient::Abort(abort_callback cb) {
  Debug("ABORT [%lu]", t_id);

  for (auto p : participants) {
      bclient[p]->Abort();
  }

  cb();
}

/* Return statistics of most recent transaction. */
vector<int> AsyncClient::Stats() {
  vector<int> v;
  return v;
}

} // namespace tapirstore
