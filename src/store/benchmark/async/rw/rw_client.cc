#include "store/benchmark/async/rw/rw_client.h"

#include <iostream>

namespace rw {

RWClient::RWClient(KeySelector *keySelector, uint64_t numKeys,
    AsyncClient &client,
    Transport &transport, uint64_t id, int numRequests, int expDuration,
    uint64_t delay, int warmupSec, int cooldownSec, int tputInterval,
    uint32_t abortBackoff, bool retryAborted, uint32_t maxBackoff, uint32_t maxAttempts,
    const std::string &latencyFilename)
    : AsyncTransactionBenchClient(client, transport, id, numRequests,
        expDuration, delay, warmupSec, cooldownSec, tputInterval, abortBackoff,
        retryAborted, maxBackoff, maxAttempts, latencyFilename), keySelector(keySelector),
        numKeys(numKeys) {
}

RWClient::~RWClient() {
}

AsyncTransaction *RWClient::GetNextTransaction() {
  RWTransaction *rw_tx = new RWTransaction(keySelector, numKeys, GetRand());
  // for(int key : rw_tx->getKeyIdxs()){
  //   //key_counts[key]++;
  //   stats.IncrementList("key distribution", key, 1);
  // }
  return rw_tx;
}

std::string RWClient::GetLastOp() const {
  return "rw";
}

} //namespace rw
