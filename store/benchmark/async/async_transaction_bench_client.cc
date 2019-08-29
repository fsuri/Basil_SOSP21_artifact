#include "store/benchmark/async/async_transaction_bench_client.h"

#include <iostream>

AsyncTransactionBenchClient::AsyncTransactionBenchClient(AsyncClient &client,
    Transport &transport, int numRequests, int expDuration, uint64_t delay,
    int warmupSec, int cooldownSec, int tputInterval, uint32_t abortBackoff,
    bool retryAborted, const std::string &latencyFilename)
    : BenchmarkClient(client, transport, numRequests, expDuration, delay,
        warmupSec, cooldownSec, tputInterval, latencyFilename),
    abortBackoff(abortBackoff), retryAborted(retryAborted),
    currTxn(nullptr), currTxnAttempts(0UL) {
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
  if (result == SUCCESS || !retryAborted) {
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
