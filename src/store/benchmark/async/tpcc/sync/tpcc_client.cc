#include "store/benchmark/async/tpcc/sync/tpcc_client.h"

#include <random>

#include "store/benchmark/async/tpcc/sync/new_order.h"
#include "store/benchmark/async/tpcc/sync/payment.h"
#include "store/benchmark/async/tpcc/sync/order_status.h"
#include "store/benchmark/async/tpcc/sync/stock_level.h"
#include "store/benchmark/async/tpcc/sync/delivery.h"

namespace tpcc {

SyncTPCCClient::SyncTPCCClient(SyncClient &client, Transport &transport,
    uint64_t seed, int numRequests, int expDuration, uint64_t delay, int warmupSec,
    int cooldownSec, int tputInterval,  uint32_t num_warehouses, uint32_t w_id,
    uint32_t C_c_id, uint32_t C_c_last, uint32_t new_order_ratio,
    uint32_t delivery_ratio, uint32_t payment_ratio, uint32_t order_status_ratio,
    uint32_t stock_level_ratio, bool static_w_id,
    uint32_t abortBackoff, bool retryAborted, uint32_t maxBackoff, uint32_t maxAttempts, uint32_t timeout,
    const std::string &latencyFilename) :
      SyncTransactionBenchClient(client, transport, seed, numRequests,
        expDuration, delay, warmupSec, cooldownSec, tputInterval, abortBackoff,
        retryAborted, maxBackoff, maxAttempts, timeout, latencyFilename),
      TPCCClient(num_warehouses, w_id, C_c_id, C_c_last, new_order_ratio,
        delivery_ratio, payment_ratio, order_status_ratio, stock_level_ratio,
        static_w_id, GetRand()) {
  stockLevelDId = std::uniform_int_distribution<uint32_t>(1, 10)(GetRand());
}

SyncTPCCClient::~SyncTPCCClient() {
}

SyncTransaction* SyncTPCCClient::GetNextTransaction() {
  uint32_t wid, did;
  TPCCTransactionType ttype = TPCCClient::GetNextTransaction(&wid, &did, GetRand());

  switch (ttype) {
    case TXN_NEW_ORDER:
      return new SyncNewOrder(GetTimeout(), wid, C_c_id, num_warehouses, GetRand());
    case TXN_PAYMENT:
      return new SyncPayment(GetTimeout(), wid, C_c_last, C_c_id, num_warehouses, GetRand());
    case TXN_ORDER_STATUS:
      return new SyncOrderStatus(GetTimeout(), wid, C_c_last, C_c_id, GetRand());
    case TXN_STOCK_LEVEL:
      return new SyncStockLevel(GetTimeout(), wid, did, GetRand());
    case TXN_DELIVERY:
      return new SyncDelivery(GetTimeout(), wid, did, GetRand());
    default:
      NOT_REACHABLE();
  }
}

std::string SyncTPCCClient::GetLastOp() const {
  return lastOp;
}


} //namespace tpcc
