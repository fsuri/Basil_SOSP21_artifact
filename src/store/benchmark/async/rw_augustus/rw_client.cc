#include "store/benchmark/async/rw_augustus/rw_client.h"

#include <iostream>

namespace rw_augustus {

RWAugustusClient::RWAugustusClient(KeySelector *keySelector, uint64_t numKeys,
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

RWAugustusClient::~RWAugustusClient() {
}

AsyncTransaction *RWAugustusClient::GetNextTransaction() {
  return new RWAugustusTransaction(keySelector, numKeys, GetRand());
}

std::string RWAugustusClient::GetLastOp() const {
  return "rw_augustus";
}

} //namespace rw_augustus
