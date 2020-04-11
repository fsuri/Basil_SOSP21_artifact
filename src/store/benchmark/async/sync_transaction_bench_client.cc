#include "store/benchmark/async/sync_transaction_bench_client.h"

#include <chrono>
#include <iostream>
#include <random>
#include <thread>

SyncTransactionBenchClient::SyncTransactionBenchClient(SyncClient &client,
    Transport &transport, uint32_t clientId, int numRequests, int expDuration,
    uint64_t delay, int warmupSec, int cooldownSec, int tputInterval,
    uint32_t abortBackoff, bool retryAborted, int32_t maxAttempts, uint32_t seed,
    const std::string &latencyFilename)
    : BenchmarkClient(transport, clientId, numRequests, expDuration, delay,
        warmupSec, cooldownSec, tputInterval, latencyFilename), client(client),
    gen(seed), abortBackoff(abortBackoff), retryAborted(retryAborted),
    maxAttempts(maxAttempts), currTxn(nullptr), currTxnAttempts(0UL) {
}

SyncTransactionBenchClient::~SyncTransactionBenchClient() {
}

void SyncTransactionBenchClient::SendNext() {
  int result;
  SendNext(&result);
}

void SyncTransactionBenchClient::SendNext(int *result) {
  currTxn = GetNextTransaction();
  currTxnAttempts = 0;
  *result = 1; // default to failure
  while (true) {
    *result = currTxn->Execute(client);
    stats.Increment(GetLastOp() + "_attempts", 1);
    ++currTxnAttempts;
    if (*result == SUCCESS || !retryAborted) {
      if (*result == SUCCESS) {
        stats.Increment(GetLastOp() + "_committed", 1);
      }
      if (*result == 1) { // RESULT_USER_ABORTED in morty-tapir/store/tapirstore/client.h
        stats.Increment(GetLastOp() +  "_" + std::to_string(*result), 1);
      }
      if (retryAborted) {
        stats.Add(GetLastOp() + "_attempts_list", currTxnAttempts);
      }
      delete currTxn;
      currTxn = nullptr;
      break;
    } else if (*result == 1) { // RESULT_USER_ABORTED in morty-tapir/store/tapirstore/client.h
      stats.Increment(GetLastOp() +  "_" + std::to_string(*result), 1);
      delete currTxn;
      currTxn = nullptr;
      break;
    } else {
      stats.Increment(GetLastOp() + "_" + std::to_string(*result), 1);
      // stats.Increment(GetLastOp() + "_attempts", 1);
      int backoff = 0;
      if (abortBackoff > 0) {
        backoff = std::uniform_int_distribution<int>(0,
          (1 << (currTxnAttempts - 1)) * abortBackoff)(gen);
        stats.Increment(GetLastOp() + "_backoff", backoff);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(backoff));
    }
  }
}

