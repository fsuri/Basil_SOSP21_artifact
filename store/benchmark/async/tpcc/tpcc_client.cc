#include "store/benchmark/async/tpcc/tpcc_client.h"

namespace tpcc {

TPCCClient::TPCCClient(Client &client, Transport &transport, int numRequests,
    int expDuration, uint64_t delay, int warmupSec, int cooldownSec,
    int tputInterval,  const std::string &latencyFilename) :
      BenchmarkClient(client, transport, numRequests, expDuration, delay,
          warmupSec, cooldownSec, tputInterval, latencyFilename) {
}

TPCCClient::~TPCCClient() {
}

void TPCCClient::SendNext() {
}

std::string TPCCClient::GetLastOp() const {
  return lastOp;
}


} //namespace tpcc
