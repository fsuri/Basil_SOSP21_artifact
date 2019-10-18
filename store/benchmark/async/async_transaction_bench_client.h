#ifndef ASYNC_TRANSACTION_BENCH_CLIENT_H
#define ASYNC_TRANSACTION_BENCH_CLIENT_H

#include <random>
#include "store/benchmark/async/bench_client.h"
#include "store/common/frontend/async_client.h"
#include "store/common/frontend/async_transaction.h"

class AsyncTransactionBenchClient : public BenchmarkClient {
public:
    AsyncTransactionBenchClient(AsyncClient &client, Transport &transport,
                                int numRequests, int expDuration, uint64_t delay, int warmupSec,
                                int cooldownSec, int tputInterval, uint32_t abortBackoff,
                                bool retryAborted, const std::string &latencyFilename = "");

    virtual ~AsyncTransactionBenchClient();

protected:
    virtual AsyncTransaction *GetNextTransaction() = 0;
    virtual void SendNext();

    void ExecuteCallback(int result,
                         std::map<std::string, std::string> readValues);

    AsyncClient &client;
private:
    uint32_t abortBackoff;
    bool retryAborted;
    AsyncTransaction *currTxn;
    size_t currTxnAttempts;
    std::mt19937 gen;

};

#endif /* ASYNC_TRANSACTION_BENCH_CLIENT_H */