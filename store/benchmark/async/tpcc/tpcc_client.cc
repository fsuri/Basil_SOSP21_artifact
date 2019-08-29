#include "store/benchmark/async/tpcc/tpcc_client.h"

#include <random>

#include "store/benchmark/async/tpcc/new_order.h"
#include "store/benchmark/async/tpcc/payment.h"
#include "store/benchmark/async/tpcc/order_status.h"
#include "store/benchmark/async/tpcc/stock_level.h"

namespace tpcc {

TPCCClient::TPCCClient(AsyncClient &client, Transport &transport,
    int numRequests, int expDuration, uint64_t delay, int warmupSec,
    int cooldownSec, int tputInterval,  uint32_t num_warehouses, uint32_t w_id,
    uint32_t C_c_id, uint32_t C_c_last, uint32_t new_order_ratio,
    uint32_t delivery_ratio, uint32_t payment_ratio, uint32_t order_status_ratio,
    uint32_t stock_level_ratio, bool static_w_id, uint32_t seed,
    uint32_t abortBackoff, bool retryAborted, const std::string &latencyFilename) :
      AsyncTransactionBenchClient(client, transport, numRequests, expDuration,
          delay, warmupSec, cooldownSec, tputInterval, abortBackoff,
          retryAborted, latencyFilename),
      num_warehouses(num_warehouses), w_id(w_id), C_c_id(C_c_id),
      C_c_last(C_c_last), new_order_ratio(new_order_ratio),
      delivery_ratio(delivery_ratio), payment_ratio(payment_ratio),
      order_status_ratio(order_status_ratio), stock_level_ratio(stock_level_ratio),
      static_w_id(static_w_id), gen(seed) {
  stockLevelDId = std::uniform_int_distribution<uint32_t>(1, 10)(gen);
}

TPCCClient::~TPCCClient() {
}

AsyncTransaction* TPCCClient::GetNextTransaction() {
  uint32_t total = new_order_ratio + delivery_ratio + payment_ratio
      + order_status_ratio + stock_level_ratio;
  uint32_t ttype = std::uniform_int_distribution<uint32_t>(0, total - 1)(gen);
  uint32_t wid;
  if (static_w_id) {
    wid = w_id;
  } else {
    wid = std::uniform_int_distribution<uint32_t>(1, num_warehouses)(gen);
  }
  if (ttype < new_order_ratio) {
    lastOp = "new_order";
    return new NewOrder(wid, C_c_id, num_warehouses, gen);
  } else if (ttype < new_order_ratio + payment_ratio) {
    lastOp = "payment";
    return new Payment(wid, C_c_last, C_c_id, num_warehouses, gen);
  } else if (ttype < new_order_ratio + payment_ratio + order_status_ratio) {
    lastOp = "order_status";
    return new OrderStatus(wid, C_c_last, C_c_id, gen);
  } else if (ttype < new_order_ratio + payment_ratio + order_status_ratio
      + stock_level_ratio) {
    uint32_t did;
    if (static_w_id) {
      did = stockLevelDId;
    } else {
      did = std::uniform_int_distribution<uint32_t>(1, num_warehouses)(gen);
    }
    lastOp = "stock_level";
    return new StockLevel(wid, did, gen);
  } else {
  }
}

std::string TPCCClient::GetLastOp() const {
  return lastOp;
}


} //namespace tpcc
