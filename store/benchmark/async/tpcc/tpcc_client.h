#ifndef TPCC_CLIENT_H
#define TPCC_CLIENT_H

#include "store/benchmark/async/bench_client.h"

namespace tpcc {

class TPCCClient : public BenchmarkClient {
 public:
  TPCCClient(Client &client, Transport &transport, int numRequests,
      uint64_t delay, int warmupSec, int tputInterval,
      const std::string &latencyFilename = "");

  virtual ~TPCCClient();

 protected:
  virtual void SendNext();

};

} //namespace tpcc

#endif /* TPCC_CLIENT_H */
