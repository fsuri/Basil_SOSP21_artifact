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
        uint32_t seed, int numRequests, int expDuration, uint64_t delay,
        int warmupSec, int cooldownSec, int tputInterval,
        int64_t abortBackoff, bool retryAborted, 
        int64_t maxBackoff, int64_t maxAttempts,
        const std::string &latencyFilename = "");

    virtual ~AsyncTransactionBenchClient();

protected:
    virtual AsyncTransaction *GetNextTransaction() = 0;
    virtual void SendNext();

    void ExecuteCallback(transaction_status_t result,
                         std::map<std::string, std::string> readValues);

    AsyncClient &client;
private:
    int64_t maxBackoff;
    int64_t abortBackoff;
    bool retryAborted;
    int64_t maxAttempts;
    AsyncTransaction *currTxn;
    int64_t currTxnAttempts;

};

#endif /* ASYNC_TRANSACTION_BENCH_CLIENT_H */
