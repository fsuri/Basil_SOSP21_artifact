/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fs435@cornell.edu>
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
#include "store/benchmark/async/sync_transaction_bench_client.h"

#include <chrono>
#include <iostream>
#include <random>
#include <thread>

SyncTransactionBenchClient::SyncTransactionBenchClient(SyncClient &client,
    Transport &transport, uint64_t id, int numRequests, int expDuration,
    uint64_t delay, int warmupSec, int cooldownSec, int tputInterval,
    uint64_t abortBackoff, bool retryAborted, uint64_t maxBackoff,
    int64_t maxAttempts, uint64_t timeout, const std::string &latencyFilename)
    : BenchmarkClient(transport, id, numRequests, expDuration, delay,
        warmupSec, cooldownSec, tputInterval, latencyFilename), client(client),
    abortBackoff(abortBackoff), retryAborted(retryAborted), maxBackoff(maxBackoff),
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
        || (maxAttempts != -1 && currTxnAttempts >= static_cast<uint64_t>(maxAttempts))
        || !retryAborted) {
      if (*result == COMMITTED) {
        stats.Increment(GetLastOp() + "_committed", 1);
      } else {
        stats.Increment(GetLastOp() +  "_" + std::to_string(*result), 1);
      }
      if (retryAborted) {
        //stats.Add(GetLastOp() + "_attempts_list", currTxnAttempts); //TODO: uncomment if need stats files
      }
      delete currTxn;
      currTxn = nullptr;
      break;
    } else {
      stats.Increment(GetLastOp() + "_" + std::to_string(*result), 1);
      uint64_t backoff = 0;
      if (abortBackoff > 0) {
        uint64_t exp = std::min(currTxnAttempts - 1UL, 56UL);
        uint64_t upper = std::min((1UL << exp) * static_cast<uint64_t>(abortBackoff),
            maxBackoff);
        backoff = std::uniform_int_distribution<uint64_t>(upper >> 1, upper)(GetRand());
        stats.Increment(GetLastOp() + "_backoff", backoff);
        Debug("Backing off for %lums", backoff);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(backoff));
    }
  }
  Debug("Transaction finished with result %d.", *result);
}
