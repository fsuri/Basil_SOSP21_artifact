#ifndef ASYNC_TPCC_CLIENT_H
#define ASYNC_TPCC_CLIENT_H

#include <random>

#include "store/benchmark/async/async_transaction_bench_client.h"
#include "store/benchmark/async/tpcc/tpcc_client.h"

namespace tpcc {

class AsyncTPCCClient : public AsyncTransactionBenchClient, public TPCCClient {
 public:
  AsyncTPCCClient(AsyncClient &client, Transport &transport, uint32_t seed, int numRequests,
      int expDuration, uint64_t delay, int warmupSec, int cooldownSec,
      int tputInterval, uint32_t num_warehouses, uint32_t w_id,
      uint32_t C_c_id, uint32_t C_c_last, uint32_t new_order_ratio,
      uint32_t delivery_ratio, uint32_t payment_ratio, uint32_t order_status_ratio,
      uint32_t stock_level_ratio, bool static_w_id,
      uint32_t abortBackoff, bool retryAborted, int32_t maxAttempts,
      const std::string &latencyFilename = "");

  virtual ~AsyncTPCCClient();

 protected:
  virtual AsyncTransaction *GetNextTransaction();
  virtual std::string GetLastOp() const;

};

} //namespace tpcc

#endif /* ASYNC_TPCC_CLIENT_H */
