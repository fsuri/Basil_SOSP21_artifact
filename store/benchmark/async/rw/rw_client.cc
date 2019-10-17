#include "store/benchmark/async/rw/rw_client.h"

// #include "store/benchmark/async/rw/add_user.h"
// #include "store/benchmark/async/rw/follow.h"
// #include "store/benchmark/async/rw/get_timeline.h"
// #include "store/benchmark/async/rw/post_tweet.h"

#include <iostream>

namespace rw {

RWClient::RWClient(KeySelector *keySelector, uint64_t numKeys,
    AsyncClient &client,
    Transport &transport, int numRequests, int expDuration, uint64_t delay,
    int warmupSec, int cooldownSec, int tputInterval, uint32_t abortBackoff,
    bool retryAborted, const std::string &latencyFilename)
    : AsyncTransactionBenchClient(client, transport, numRequests, expDuration,
        delay, warmupSec, cooldownSec, tputInterval, abortBackoff,
        retryAborted, latencyFilename), keySelector(keySelector) {
}

RWClient::~RWClient() {
}

AsyncTransaction *RWClient::GetNextTransaction() {
  tid++;
  return new RWTransaction(tid, keySelector, numKeys);
}

std::string RWClient::GetLastOp() const {
  return "rw";
}

} //namespace rw
