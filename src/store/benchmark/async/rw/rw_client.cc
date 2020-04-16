#include "store/benchmark/async/rw/rw_client.h"

#include <iostream>

namespace rw {

RWClient::RWClient(KeySelector *keySelector, uint64_t numKeys,
    AsyncClient &client,
    Transport &transport, uint32_t seed, int numRequests, int expDuration,
    uint64_t delay, int warmupSec, int cooldownSec, int tputInterval,
    uint32_t abortBackoff, bool retryAborted, int32_t maxAttempts,
    const std::string &latencyFilename)
    : AsyncTransactionBenchClient(client, transport, seed, numRequests,
        expDuration, delay, warmupSec, cooldownSec, tputInterval, abortBackoff,
        retryAborted, maxAttempts, latencyFilename), keySelector(keySelector),
        numKeys(numKeys) {
}

RWClient::~RWClient() {
}

AsyncTransaction *RWClient::GetNextTransaction() {
  return new RWTransaction(keySelector, numKeys, GetRand());
}

std::string RWClient::GetLastOp() const {
  return "rw";
}

} //namespace rw
