#include "store/benchmark/async/tpcc/tpcc_client.h"

namespace tpcc {

TPCCClient::TPCCClient(Client &client, Transport &transport, int numRequests,
    uint64_t delay, int warmupSec, int tputInterval,
    const std::string &latencyFilename) : BenchmarkClient(client, transport,
      numRequests, delay, warmupSec, tputInterval, latencyFilename) {
}

TPCCClient::~TPCCClient() {
}

void TPCCClient::SendNext() {
}

std::string TPCCClient::GetLastOp() const {
  return lastOp;
}


} //namespace tpcc
