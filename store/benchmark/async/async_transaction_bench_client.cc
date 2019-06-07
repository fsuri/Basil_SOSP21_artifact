#include "store/benchmark/async/async_transaction_bench_client.h"

#include <iostream>

AsyncTransactionBenchClient::AsyncTransactionBenchClient(AsyncClient &client,
    Transport &transport, int numRequests, int expDuration, uint64_t delay,
    int warmupSec, int cooldownSec, int tputInterval,
    const std::string &latencyFilename)
    : BenchmarkClient(client, transport, numRequests, expDuration, delay,
        warmupSec, cooldownSec, tputInterval, latencyFilename),
    currTxn(nullptr) {
}

AsyncTransactionBenchClient::~AsyncTransactionBenchClient() {
}

void AsyncTransactionBenchClient::SendNext() {
  currTxn = GetNextTransaction();
  client.Execute(currTxn, [this](bool committed,
        std::map<std::string, std::string> readValues){
    delete this->currTxn;
    this->currTxn = nullptr;
    this->OnReply(committed);
  });
}
