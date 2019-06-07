#include "store/benchmark/async/tpcc/tpcc_client.h"

#include <random>

#include "store/benchmark/async/tpcc/new_order.h"

namespace tpcc {

TPCCClient::TPCCClient(AsyncClient &client, Transport &transport,
    int numRequests, int expDuration, uint64_t delay, int warmupSec,
    int cooldownSec, int tputInterval,  const std::string &latencyFilename) :
      AsyncTransactionBenchClient(client, transport, numRequests, expDuration,
          delay, warmupSec, cooldownSec, tputInterval, latencyFilename) {
}

TPCCClient::~TPCCClient() {
}

AsyncTransaction* TPCCClient::GetNextTransaction() {
  lastOp = "new_order";
  return new NewOrder(1, 0, 1, gen);
}

std::string TPCCClient::GetLastOp() const {
  return lastOp;
}


} //namespace tpcc
