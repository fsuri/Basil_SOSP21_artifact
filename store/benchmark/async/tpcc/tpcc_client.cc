#include "store/benchmark/async/tpcc/tpcc_client.h"

#include <random>

#include "store/benchmark/async/tpcc/new_order.h"

namespace tpcc {

TPCCClient::TPCCClient(AsyncClient &client, Transport &transport,
    int numRequests, int expDuration, uint64_t delay, int warmupSec,
    int cooldownSec, int tputInterval,  uint32_t num_warehouses, uint32_t w_id,
    uint32_t C_c_id, const std::string &latencyFilename) :
      AsyncTransactionBenchClient(client, transport, numRequests, expDuration,
          delay, warmupSec, cooldownSec, tputInterval, latencyFilename),
      num_warehouses(num_warehouses), w_id(w_id), C_c_id(C_c_id) {
}

TPCCClient::~TPCCClient() {
}

AsyncTransaction* TPCCClient::GetNextTransaction() {
  lastOp = "new_order";
  return new NewOrder(w_id, C_c_id, num_warehouses, gen);
}

std::string TPCCClient::GetLastOp() const {
  return lastOp;
}


} //namespace tpcc
