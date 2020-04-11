#ifndef SYNC_TRANSACTION_BENCH_CLIENT_H
#define SYNC_TRANSACTION_BENCH_CLIENT_H

#include "store/benchmark/async/bench_client.h"
#include "store/common/frontend/sync_client.h"
#include "store/common/frontend/sync_transaction.h"

#include <random>

class SyncTransactionBenchClient : public BenchmarkClient {
 public:
  SyncTransactionBenchClient(SyncClient &client, Transport &transport,
      uint32_t clientId, int numRequests, int expDuration, uint64_t delay, int warmupSec,
      int cooldownSec, int tputInterval, uint32_t abortBackoff,
      bool retryAborted, int32_t maxAttempts, uint32_t seed,
      const std::string &latencyFilename = "");

  virtual ~SyncTransactionBenchClient();

  void SendNext(int *result);
 protected:
  virtual SyncTransaction *GetNextTransaction() = 0;
  virtual void SendNext() override;

  SyncClient &client;
  std::mt19937 gen;
 private:
  uint32_t abortBackoff;
  bool retryAborted;
  int32_t maxAttempts;
  SyncTransaction *currTxn;
  size_t currTxnAttempts;

};

#endif /* SYNC_TRANSACTION_BENCH_CLIENT_H */
