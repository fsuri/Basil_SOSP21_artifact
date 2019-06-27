#ifndef ASYNC_TRANSACTION_BENCH_CLIENT_H
#define ASYNC_TRANSACTION_BENCH_CLIENT_H

#include "store/benchmark/async/bench_client.h"
#include "store/common/frontend/async_client.h"
#include "store/common/frontend/async_transaction.h"

class AsyncTransactionBenchClient : public BenchmarkClient {
 public:
  AsyncTransactionBenchClient(AsyncClient &client, Transport &transport,
      int numRequests, int expDuration, uint64_t delay, int warmupSec,
      int cooldownSec, int tputInterval, bool abortBackoff,
      const std::string &latencyFilename = "");

  virtual ~AsyncTransactionBenchClient();

 protected:
  virtual AsyncTransaction *GetNextTransaction() = 0;
  virtual void SendNext();

  void ExecuteCallback(int result,
      std::map<std::string, std::string> readValues);

 private:
  bool abortBackoff;
  AsyncTransaction *currTxn;
  size_t currTxnAttempts;
  std::mt19937 gen;

};

#endif /* ASYNC_TRANSACTION_BENCH_CLIENT_H */
