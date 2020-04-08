#include "store/benchmark/async/tpcc/sync/tpcc_client.h"

#include <random>

#include "store/benchmark/async/tpcc/sync/new_order.h"
#include "store/benchmark/async/tpcc/sync/payment.h"
#include "store/benchmark/async/tpcc/sync/order_status.h"
#include "store/benchmark/async/tpcc/sync/stock_level.h"
#include "store/benchmark/async/tpcc/sync/delivery.h"

namespace tpcc {

SyncTPCCClient::SyncTPCCClient(SyncClient &client, Transport &transport,
    uint32_t clientId, int numRequests, int expDuration, uint64_t delay, int warmupSec,
    int cooldownSec, int tputInterval,  uint32_t num_warehouses, uint32_t w_id,
    uint32_t C_c_id, uint32_t C_c_last, uint32_t new_order_ratio,
    uint32_t delivery_ratio, uint32_t payment_ratio, uint32_t order_status_ratio,
    uint32_t stock_level_ratio, bool static_w_id, uint32_t seed,
    uint32_t abortBackoff, bool retryAborted, int32_t maxAttempts,
    const std::string &latencyFilename) :
      SyncTransactionBenchClient(client, transport, clientId, numRequests,
        expDuration, delay, warmupSec, cooldownSec, tputInterval, abortBackoff,
        retryAborted, maxAttempts, seed, latencyFilename),
      TPCCClient(num_warehouses, w_id, C_c_id, C_c_last, new_order_ratio,
        delivery_ratio, payment_ratio, order_status_ratio, stock_level_ratio,
        static_w_id, gen) {
  stockLevelDId = std::uniform_int_distribution<uint32_t>(1, 10)(gen);
}

SyncTPCCClient::~SyncTPCCClient() {
}

SyncTransaction* SyncTPCCClient::GetNextTransaction() {
  uint32_t wid, did;
  TPCCTransactionType ttype = TPCCClient::GetNextTransaction(&wid, &did, gen);
  switch (ttype) {
    case TXN_NEW_ORDER:
      return new SyncNewOrder(wid, C_c_id, num_warehouses, gen);
    case TXN_PAYMENT:
      return new SyncPayment(wid, C_c_last, C_c_id, num_warehouses, gen);
    case TXN_ORDER_STATUS:
      return new SyncOrderStatus(wid, C_c_last, C_c_id, gen);
    case TXN_STOCK_LEVEL:
      return new SyncStockLevel(wid, did, gen);
    case TXN_DELIVERY:
      return new SyncDelivery(wid, did, gen);
    default:
      NOT_REACHABLE();
  }
}

std::string SyncTPCCClient::GetLastOp() const {
  return lastOp;
}


} //namespace tpcc
