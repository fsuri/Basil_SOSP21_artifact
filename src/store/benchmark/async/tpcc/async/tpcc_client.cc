#include "store/benchmark/async/tpcc/async/tpcc_client.h"

#include <random>

#include "store/benchmark/async/tpcc/async/new_order.h"
#include "store/benchmark/async/tpcc/async/payment.h"
#include "store/benchmark/async/tpcc/async/order_status.h"
#include "store/benchmark/async/tpcc/async/stock_level.h"

namespace tpcc {

AsyncTPCCClient::AsyncTPCCClient(AsyncClient &client, Transport &transport,
    uint32_t seed,
    int numRequests, int expDuration, uint64_t delay, int warmupSec,
    int cooldownSec, int tputInterval,  uint32_t num_warehouses, uint32_t w_id,
    uint32_t C_c_id, uint32_t C_c_last, uint32_t new_order_ratio,
    uint32_t delivery_ratio, uint32_t payment_ratio, uint32_t order_status_ratio,
    uint32_t stock_level_ratio, bool static_w_id,
    uint32_t abortBackoff, bool retryAborted, int32_t maxAttempts,
    const std::string &latencyFilename) :
      AsyncTransactionBenchClient(client, transport, seed, numRequests, expDuration,
          delay, warmupSec, cooldownSec, tputInterval, abortBackoff,
          retryAborted, maxAttempts, latencyFilename),
      TPCCClient(num_warehouses, w_id, C_c_id, C_c_last, new_order_ratio,
          delivery_ratio, payment_ratio, order_status_ratio, stock_level_ratio,
          static_w_id, GetRand()) {
  stockLevelDId = std::uniform_int_distribution<uint32_t>(1, 10)(GetRand());
}

AsyncTPCCClient::~AsyncTPCCClient() {
}

AsyncTransaction* AsyncTPCCClient::GetNextTransaction() {
  uint32_t wid, did;
  TPCCTransactionType ttype = TPCCClient::GetNextTransaction(&wid, &did, GetRand());
  Debug("w_id: %u; num_warehouses: %u", wid, num_warehouses);
  switch (ttype) {
    case TXN_NEW_ORDER:
      return new AsyncNewOrder(wid, C_c_id, num_warehouses, GetRand());
    case TXN_PAYMENT:
      return new AsyncPayment(wid, C_c_last, C_c_id, num_warehouses, GetRand());
    case TXN_ORDER_STATUS:
      return new AsyncOrderStatus(wid, C_c_last, C_c_id, GetRand());
    case TXN_STOCK_LEVEL:
      return new AsyncStockLevel(wid, did, GetRand());
    case TXN_DELIVERY:
      return nullptr; //new AsyncDelivery(wid, TPCCClient::GetRand());
    default:
      NOT_REACHABLE();
  }

}

std::string AsyncTPCCClient::GetLastOp() const {
  return lastOp;
}


} //namespace tpcc
