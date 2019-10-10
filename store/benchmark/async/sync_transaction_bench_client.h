#ifndef SYNC_TRANSACTION_BENCH_CLIENT_H
#define SYNC_TRANSACTION_BENCH_CLIENT_H

#include "store/benchmark/async/bench_client.h"
#include "store/common/frontend/sync_client.h"
#include "store/common/frontend/sync_transaction.h"

#include <random>

class SyncTransactionBenchClient : public BenchmarkClient {
 public:
  SyncTransactionBenchClient(SyncClient &client, Transport &transport,
      int numRequests, int expDuration, uint64_t delay, int warmupSec,
      int cooldownSec, int tputInterval, uint32_t abortBackoff,
      bool retryAborted, const std::string &latencyFilename = "");

  virtual ~SyncTransactionBenchClient();

 protected:
  virtual SyncTransaction *GetNextTransaction() = 0;
  virtual void SendNext();

  SyncClient &client;
 private:
  uint32_t abortBackoff;
  bool retryAborted;
  SyncTransaction *currTxn;
  size_t currTxnAttempts;
  std::mt19937 gen;

};

#endif /* SYNC_TRANSACTION_BENCH_CLIENT_H */
