#include "store/benchmark/async/sync_transaction_bench_client.h"

#include <chrono>
#include <iostream>
#include <random>
#include <thread>

SyncTransactionBenchClient::SyncTransactionBenchClient(SyncClient &client,
    Transport &transport, uint32_t seed, int numRequests, int expDuration,
    uint64_t delay, int warmupSec, int cooldownSec, int tputInterval,
    uint32_t abortBackoff, bool retryAborted, int32_t maxAttempts,
    uint32_t timeout,
    const std::string &latencyFilename)
    : BenchmarkClient(transport, seed, numRequests, expDuration, delay,
        warmupSec, cooldownSec, tputInterval, latencyFilename), client(client),
    abortBackoff(abortBackoff), retryAborted(retryAborted),
    maxAttempts(maxAttempts), timeout(timeout), currTxn(nullptr),
    currTxnAttempts(0UL) {
}

SyncTransactionBenchClient::~SyncTransactionBenchClient() {
}

void SyncTransactionBenchClient::SendNext() {
  transaction_status_t result;
  SendNext(&result);
}

void SyncTransactionBenchClient::SendNext(transaction_status_t *result) {
  currTxn = GetNextTransaction();
  currTxnAttempts = 0;
  *result = ABORTED_SYSTEM; // default to failure
  while (true) {
    *result = currTxn->Execute(client);
    stats.Increment(GetLastOp() + "_attempts", 1);
    ++currTxnAttempts;
    if (*result == COMMITTED || *result == ABORTED_USER
        || (maxAttempts != -1 && currTxnAttempts >= maxAttempts)
        || !retryAborted) {
      if (*result == COMMITTED) {
        stats.Increment(GetLastOp() + "_committed", 1);
      } else {
        stats.Increment(GetLastOp() +  "_" + std::to_string(*result), 1);
      }
      if (retryAborted) {
        stats.Add(GetLastOp() + "_attempts_list", currTxnAttempts);
      }
      delete currTxn;
      currTxn = nullptr;
      break;
    } else {
      stats.Increment(GetLastOp() + "_" + std::to_string(*result), 1);
      int backoff = 0;
      if (abortBackoff > 0) {
        backoff = std::uniform_int_distribution<int>(0,
          (1 << (currTxnAttempts - 1)) * abortBackoff)(GetRand());
        stats.Increment(GetLastOp() + "_backoff", backoff);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(backoff));
    }
  }
}

