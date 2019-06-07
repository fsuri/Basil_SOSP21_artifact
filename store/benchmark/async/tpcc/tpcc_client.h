#ifndef TPCC_CLIENT_H
#define TPCC_CLIENT_H

#include <random>

#include "store/benchmark/async/async_transaction_bench_client.h"

namespace tpcc {

class TPCCClient : public AsyncTransactionBenchClient {
 public:
  TPCCClient(AsyncClient &client, Transport &transport, int numRequests,
      int expDuration, uint64_t delay, int warmupSec, int cooldownSec,
      int tputInterval, const std::string &latencyFilename = "");

  virtual ~TPCCClient();

 protected:
  virtual AsyncTransaction *GetNextTransaction();
  virtual std::string GetLastOp() const;

 private:
  std::string lastOp;
  std::mt19937 gen;

};

} //namespace tpcc

#endif /* TPCC_CLIENT_H */
