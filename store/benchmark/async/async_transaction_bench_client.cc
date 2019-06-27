#include "store/benchmark/async/async_transaction_bench_client.h"

#include <iostream>

AsyncTransactionBenchClient::AsyncTransactionBenchClient(AsyncClient &client,
    Transport &transport, int numRequests, int expDuration, uint64_t delay,
    int warmupSec, int cooldownSec, int tputInterval, bool abortBackoff,
    const std::string &latencyFilename)
    : BenchmarkClient(client, transport, numRequests, expDuration, delay,
        warmupSec, cooldownSec, tputInterval, latencyFilename),
    abortBackoff(abortBackoff), currTxn(nullptr), currTxnAttempts(0UL) {
}

AsyncTransactionBenchClient::~AsyncTransactionBenchClient() {
}

void AsyncTransactionBenchClient::SendNext() {
  currTxn = GetNextTransaction();
  currTxnAttempts = 0;
  stats.Increment(GetLastOp() + "_attempts", 1);
  client.Execute(currTxn,
      std::bind(&AsyncTransactionBenchClient::ExecuteCallback, this,
        std::placeholders::_1, std::placeholders::_2));
}

void AsyncTransactionBenchClient::ExecuteCallback(int result,
    std::map<std::string, std::string> readValues) {
  if (result == SUCCESS) {
    stats.Increment(GetLastOp() + "_committed", 1);
    delete currTxn;
    currTxn = nullptr;
    OnReply(result);
  } else {
    stats.Increment(GetLastOp() + "_" + std::to_string(result), 1);
    stats.Increment(GetLastOp() + "_attempts", 1);
    int backoff = 0;
    if (abortBackoff) {
      backoff = std::uniform_int_distribution<int>(0,
        (1 << currTxnAttempts) * 1000)(gen);
      stats.Increment(GetLastOp() + "_backoff", backoff);
    }
    ++currTxnAttempts;
    transport.Timer(backoff, [this]() {
      client.Execute(currTxn,
          std::bind(&AsyncTransactionBenchClient::ExecuteCallback, this,
            std::placeholders::_1, std::placeholders::_2));
    });
  }
}
