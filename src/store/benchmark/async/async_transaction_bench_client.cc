#include "store/benchmark/async/async_transaction_bench_client.h"

#include <iostream>
#include <random>

AsyncTransactionBenchClient::AsyncTransactionBenchClient(AsyncClient &client,
    Transport &transport, uint32_t clientId, int numRequests, int expDuration,
    uint64_t delay, int warmupSec, int cooldownSec, int tputInterval,
    uint32_t abortBackoff, bool retryAborted, int32_t maxAttempts, uint32_t seed,
    const std::string &latencyFilename) : BenchmarkClient(transport, clientId,
      numRequests, expDuration, delay, warmupSec, cooldownSec, tputInterval,
      latencyFilename), client(client), gen(seed), abortBackoff(abortBackoff),
      retryAborted(retryAborted), maxAttempts(maxAttempts), currTxn(nullptr),
      currTxnAttempts(0UL) {
}

AsyncTransactionBenchClient::~AsyncTransactionBenchClient() {
}

void AsyncTransactionBenchClient::SendNext() {
  currTxn = GetNextTransaction();
  currTxnAttempts = 0;
  client.Execute(currTxn,
      std::bind(&AsyncTransactionBenchClient::ExecuteCallback, this,
        std::placeholders::_1, std::placeholders::_2));
}

void AsyncTransactionBenchClient::ExecuteCallback(int result,
    std::map<std::string, std::string> readValues) {
  stats.Increment(GetLastOp() + "_attempts", 1);
  ++currTxnAttempts;
  if (result == SUCCESS ||
      (maxAttempts != -1 && currTxnAttempts >= maxAttempts) ||
      !retryAborted) {
    if (result == SUCCESS) {
      stats.Increment(GetLastOp() + "_committed", 1);
    }
    if (retryAborted) {
      stats.Add(GetLastOp() + "_attempts_list", currTxnAttempts);
    }
    delete currTxn;
    currTxn = nullptr;
    OnReply(result);
  } else {
    stats.Increment(GetLastOp() + "_" + std::to_string(result), 1);
    stats.Increment(GetLastOp() + "_attempts", 1);
    int backoff = 0;
    if (abortBackoff > 0) {
      backoff = std::uniform_int_distribution<int>(0,
          (1 << (currTxnAttempts - 1)) * abortBackoff)(gen);
      stats.Increment(GetLastOp() + "_backoff", backoff);
    }
    transport.Timer(backoff, [this]() {
        client.Execute(currTxn,
            std::bind(&AsyncTransactionBenchClient::ExecuteCallback, this,
            std::placeholders::_1, std::placeholders::_2));
        });
  }
}
