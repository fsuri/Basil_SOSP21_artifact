#ifndef TPCC_CLIENT_H
#define TPCC_CLIENT_H

#include "store/benchmark/async/bench_client.h"

namespace tpcc {

class TPCCClient : public BenchmarkClient {
 public:
  TPCCClient(Client &client, Transport &transport, int numRequests,
      int expDuration, uint64_t delay, int warmupSec, int cooldownSec,
      int tputInterval, const std::string &latencyFilename = "");

  virtual ~TPCCClient();

 protected:
  virtual void SendNext();
  virtual std::string GetLastOp() const;

 private:
  std::string lastOp;

};

} //namespace tpcc

#endif /* TPCC_CLIENT_H */
