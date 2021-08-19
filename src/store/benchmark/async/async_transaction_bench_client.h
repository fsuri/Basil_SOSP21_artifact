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
#ifndef ASYNC_TRANSACTION_BENCH_CLIENT_H
#define ASYNC_TRANSACTION_BENCH_CLIENT_H

#include <random>
#include "store/benchmark/async/bench_client.h"
#include "store/common/frontend/client.h"
#include "store/common/frontend/async_client.h"
#include "store/common/frontend/async_transaction.h"

class AsyncTransactionBenchClient : public BenchmarkClient {
public:
    AsyncTransactionBenchClient(AsyncClient &client, Transport &transport,
        uint64_t id, int numRequests, int expDuration, uint64_t delay,
        int warmupSec, int cooldownSec, int tputInterval,
        uint64_t abortBackoff, bool retryAborted, 
        uint64_t maxBackoff, int64_t maxAttempts,
        const std::string &latencyFilename = "");

    virtual ~AsyncTransactionBenchClient();

protected:
    virtual AsyncTransaction *GetNextTransaction() = 0;
    virtual void SendNext();

    void ExecuteCallback(transaction_status_t result,
                         std::map<std::string, std::string> readValues);

    AsyncClient &client;
private:
    uint64_t maxBackoff;
    uint64_t abortBackoff;
    bool retryAborted;
    int64_t maxAttempts;
    AsyncTransaction *currTxn;
    uint64_t currTxnAttempts;

};

#endif /* ASYNC_TRANSACTION_BENCH_CLIENT_H */
