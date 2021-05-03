#include "store/benchmark/async/async_transaction_bench_client.h"

#include <iostream>
#include <random>

AsyncTransactionBenchClient::AsyncTransactionBenchClient(AsyncClient &client,
    Transport &transport, uint64_t id, int numRequests, int expDuration,
    uint64_t delay, int warmupSec, int cooldownSec, int tputInterval,
    uint64_t abortBackoff, bool retryAborted, uint64_t maxBackoff,
    int64_t maxAttempts, const std::string &latencyFilename) :
      BenchmarkClient(transport, id,
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
      (maxAttempts != -1 && currTxnAttempts >= static_cast<uint64_t>(maxAttempts)) ||
      !retryAborted) {
    if (result == COMMITTED) {
      stats.Increment(GetLastOp() + "_committed", 1);
    }
    if (retryAborted) {
      //stats.Add(GetLastOp() + "_attempts_list", currTxnAttempts);  //TODO: uncomment if want to collect attempt stats
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
      Debug("Exp is %lu (min of %lu and 56.", exp, currTxnAttempts - 1UL);
      uint64_t upper = std::min((1UL << exp) * abortBackoff, maxBackoff);
      Debug("Upper is %lu (min of %lu and %lu.", upper, (1UL << exp) * abortBackoff,
          maxBackoff);
      backoff = std::uniform_int_distribution<uint64_t>(0UL, upper)(GetRand());
      stats.Increment(GetLastOp() + "_backoff", backoff);
      Debug("Backing off for %lums", backoff);
    }
    //std::cerr << "Backing off for ms: " << backoff << std::endl;
    transport.Timer(backoff, [this]() {
        client.Execute(currTxn,
            std::bind(&AsyncTransactionBenchClient::ExecuteCallback, this,
            std::placeholders::_1, std::placeholders::_2));
        });
  }
}
