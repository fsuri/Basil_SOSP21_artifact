/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fsp@cs.cornell.edu>
 *                Matthew Burke <matthelb@cs.cornell.edu>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************************/
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
    transport.Timer(backoff, [this]() {
        client.Execute(currTxn,
            std::bind(&AsyncTransactionBenchClient::ExecuteCallback, this,
            std::placeholders::_1, std::placeholders::_2));
        });
  }
}
