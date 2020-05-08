#ifndef SYNC_TRANSACTION_BENCH_CLIENT_H
#define SYNC_TRANSACTION_BENCH_CLIENT_H

#include "store/benchmark/async/bench_client.h"
#include "store/common/frontend/sync_client.h"
#include "store/common/frontend/sync_transaction.h"

#include <random>

class SyncTransactionBenchClient : public BenchmarkClient {
 public:
  SyncTransactionBenchClient(SyncClient &client, Transport &transport,
      uint32_t seed, int numRequests, int expDuration, uint64_t delay,
      int warmupSec,
      int cooldownSec, int tputInterval, uint64_t abortBackoff,
      bool retryAborted, uint64_t maxBackoff, int64_t maxAttempts, uint64_t timeout,
      const std::string &latencyFilename = "");

  virtual ~SyncTransactionBenchClient();

  void SendNext(transaction_status_t *result);
 protected:
  virtual SyncTransaction *GetNextTransaction() = 0;
  virtual void SendNext() override;
  inline uint32_t GetTimeout() const { return timeout; } 

  SyncClient &client;
 private:
  uint64_t abortBackoff;
  bool retryAborted;
  uint64_t maxBackoff;
  int64_t maxAttempts;
  uint64_t timeout;
  SyncTransaction *currTxn;
  int64_t currTxnAttempts;

};

#endif /* SYNC_TRANSACTION_BENCH_CLIENT_H */
