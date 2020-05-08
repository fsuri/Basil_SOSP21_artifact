#include "store/benchmark/async/sync_transaction_bench_client.h"

#include <chrono>
#include <iostream>
#include <random>
#include <thread>

SyncTransactionBenchClient::SyncTransactionBenchClient(SyncClient &client,
    Transport &transport, uint32_t seed, int numRequests, int expDuration,
    uint64_t delay, int warmupSec, int cooldownSec, int tputInterval,
    uint64_t abortBackoff, bool retryAborted, uint64_t maxBackoff,
    int64_t maxAttempts, uint64_t timeout, const std::string &latencyFilename)
    : BenchmarkClient(transport, seed, numRequests, expDuration, delay,
        warmupSec, cooldownSec, tputInterval, latencyFilename), client(client),
    abortBackoff(abortBackoff), retryAborted(retryAborted), maxBackoff(maxBackoff),
    maxAttempts(maxAttempts), timeout(timeout), currTxn(nullptr),
    currTxnAttempts(0L) {
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
      uint64_t backoff = 0;
      if (abortBackoff > 0) {
        uint64_t exp = std::min(currTxnAttempts - 1UL, 56UL);
        uint64_t upper = std::min((1 << exp) * abortBackoff, maxBackoff);
        backoff = std::uniform_int_distribution<uint64_t>(upper >> 1, upper)(GetRand());
        stats.Increment(GetLastOp() + "_backoff", backoff);
        Warning("Backing off for %lums", backoff);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(backoff));
    }
  }
}

