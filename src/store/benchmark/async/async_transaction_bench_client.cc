#include "store/benchmark/async/async_transaction_bench_client.h"

#include <iostream>
#include <random>

AsyncTransactionBenchClient::AsyncTransactionBenchClient(AsyncClient &client,
    Transport &transport, uint32_t seed, int numRequests, int expDuration,
    uint64_t delay, int warmupSec, int cooldownSec, int tputInterval,
    int64_t abortBackoff, bool retryAborted, int64_t maxBackoff,
    int64_t maxAttempts, const std::string &latencyFilename) :
      BenchmarkClient(transport, seed,
      numRequests, expDuration, delay, warmupSec, cooldownSec, tputInterval,
      latencyFilename), client(client), maxBackoff(maxBackoff), abortBackoff(abortBackoff),
      retryAborted(retryAborted), maxAttempts(maxAttempts), currTxn(nullptr),
      currTxnAttempts(0UL) {
}

AsyncTransactionBenchClient::~AsyncTransactionBenchClient() {
}

void AsyncTransactionBenchClient::SendNext() {
  currTxn = GetNextTransaction();
  Latency_Start(&latency);
  currTxnAttempts = 0;
  client.Execute(currTxn,
      std::bind(&AsyncTransactionBenchClient::ExecuteCallback, this,
        std::placeholders::_1, std::placeholders::_2));
}

void AsyncTransactionBenchClient::ExecuteCallback(transaction_status_t result,
    std::map<std::string, std::string> readValues) {
  Debug("ExecuteCallback with result %d.", result);
  stats.Increment(GetLastOp() + "_attempts", 1);
  ++currTxnAttempts;
  if (result == COMMITTED || result == ABORTED_USER || 
      (maxAttempts != -1 && currTxnAttempts >= maxAttempts) ||
      !retryAborted) {
    if (result == COMMITTED) {
      stats.Increment(GetLastOp() + "_committed", 1);
    }
    if (retryAborted) {
      stats.Add(GetLastOp() + "_attempts_list", currTxnAttempts);
    }
    delete currTxn;
    currTxn = nullptr;
    Debug("Moving on to next.");
    OnReply(result);
  } else {
    stats.Increment(GetLastOp() + "_" + std::to_string(result), 1);
    uint64_t backoff = 0;
    if (abortBackoff > 0) {
      uint64_t exp = std::min(currTxnAttempts - 1UL, 56UL);
      uint64_t upper = std::min((1 << exp) * abortBackoff, maxBackoff);
      backoff = std::uniform_int_distribution<uint64_t>(upper >> 1, upper)(GetRand());
      stats.Increment(GetLastOp() + "_backoff", backoff);
      Warning("Backing off for %lums", backoff);
    }
    transport.Timer(backoff, [this]() {
        client.Execute(currTxn,
            std::bind(&AsyncTransactionBenchClient::ExecuteCallback, this,
            std::placeholders::_1, std::placeholders::_2));
        });
  }
}
