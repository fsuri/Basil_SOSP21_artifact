// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * store/qwstore/client.cc:
 *   Single QWstore client. Implements the API functionalities.
 *
 * Copyright 2015 Irene Zhang <iyzhang@cs.washington.edu>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************************/

#include "store/weakstore/client.h"

namespace weakstore {

using namespace std;
using namespace proto;

Client::Client(string configPath, int nShards, int closestReplica,
    partitioner part) : transport(0.0, 0.0, 0), part(part) {
    // Initialize all state here;
    client_id = 0;
    while (client_id == 0) {
        random_device rd;
        mt19937_64 gen(rd());
        uniform_int_distribution<uint64_t> dis;
        client_id = dis(gen);
    }
    
    nshards = nShards;
    bclient.reserve(nShards);

    Debug("Initializing WeakStore client with id [%lu]", client_id);

    /* Start a client for each shard. */
    for (int i = 0; i < nShards; i++) {
        string shardConfigPath = configPath + to_string(i) + ".config";
        bclient[i] = new ShardClient(shardConfigPath, &transport,
            client_id, i, closestReplica);
    }

    /* Run the transport in a new thread. */
    clientTransport = new thread(&Client::run_client, this);

    Debug("WeakStore client [%lu] created!", client_id);
}

Client::~Client()
{
    transport.Stop();
    for (auto b : bclient) {
        delete b;
    }
    clientTransport->join();
}

/* Runs the transport event loop. */
void
Client::run_client()
{
    transport.Run();
}

void Client::Get(const std::string &key, get_callback gcb,
    get_timeout_callback gtcb, uint32_t timeout) {
  Debug("GET Operation [%s]", key.c_str());

  // Contact the appropriate shard to get the value.
  std::vector<int> txnGroups;
  int i = part(key, nshards, -1, txnGroups);


  // Send the GET operation to appropriate shard.
  Promise promise;

  bclient[i]->Get(client_id, key, &promise);
  // TODO: broken
}

void Client::Put(const std::string &key, const std::string &value,
    put_callback pcb, put_timeout_callback ptcb, uint32_t timeout) {
  Debug("PUT Operation [%s]", key.c_str());

  // Contact the appropriate shard to set the value.
  std::vector<int> txnGroups;
  int i = part(key, nshards, -1, txnGroups);

     // Send the GET operation to appropriate shard.
  Promise promise;

  bclient[i]->Put(client_id, key, value, &promise);
  // TODO: broken
}

void Client::Commit(commit_callback ccb, commit_timeout_callback ctcb,
    uint32_t timeout) {
}
  
void Client::Abort(abort_callback acb, abort_timeout_callback atcb,
    uint32_t timeout) {
}

vector<int> Client::Stats() {
  vector<int> v;
  return v;
}

} // namespace weakstore
