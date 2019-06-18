#ifndef TPCC_CLIENT_H
#define TPCC_CLIENT_H

#include <random>

#include "store/benchmark/async/async_transaction_bench_client.h"

namespace tpcc {

class TPCCClient : public AsyncTransactionBenchClient {
 public:
  TPCCClient(AsyncClient &client, Transport &transport, int numRequests,
      int expDuration, uint64_t delay, int warmupSec, int cooldownSec,
      int tputInterval, uint32_t num_warehouses, uint32_t w_id,
      uint32_t C_c_id, uint32_t C_c_last,
      const std::string &latencyFilename = "");

  virtual ~TPCCClient();

 protected:
  virtual AsyncTransaction *GetNextTransaction();
  virtual std::string GetLastOp() const;

 private:
  uint32_t num_warehouses;
  uint32_t w_id;
  uint32_t C_c_id;
  uint32_t C_c_last;
  std::string lastOp;
  std::mt19937 gen;

};

} //namespace tpcc

#endif /* TPCC_CLIENT_H */
