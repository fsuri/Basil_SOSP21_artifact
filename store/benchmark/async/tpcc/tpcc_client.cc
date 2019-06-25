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
    uint32_t stock_level_ratio, const std::string &latencyFilename) :
      AsyncTransactionBenchClient(client, transport, numRequests, expDuration,
          delay, warmupSec, cooldownSec, tputInterval, latencyFilename),
      num_warehouses(num_warehouses), w_id(w_id), C_c_id(C_c_id),
      C_c_last(C_c_last), new_order_ratio(new_order_ratio),
      delivery_ratio(delivery_ratio), payment_ratio(payment_ratio),
      order_status_ratio(order_status_ratio), stock_level_ratio(stock_level_ratio) {
  stockLevelDId = std::uniform_int_distribution<uint32_t>(1, 10)(gen);
}

TPCCClient::~TPCCClient() {
}

AsyncTransaction* TPCCClient::GetNextTransaction() {
  uint32_t total = new_order_ratio + delivery_ratio + payment_ratio
      + order_status_ratio + stock_level_ratio;
  uint32_t ttype = std::rand() % total;
  if (ttype < new_order_ratio) {
    lastOp = "new_order";
    return new NewOrder(w_id, C_c_id, num_warehouses, gen);
  } else if (ttype < new_order_ratio + payment_ratio) {
    lastOp = "payment";
    return new Payment(w_id, C_c_last, C_c_id, num_warehouses, gen);
  } else if (ttype < new_order_ratio + payment_ratio + order_status_ratio) {
    lastOp = "order_status";
    return new OrderStatus(w_id, C_c_last, C_c_id, gen);
  } else if (ttype < new_order_ratio + payment_ratio + order_status_ratio
      + stock_level_ratio) {
    lastOp = "stock_level";
    return new StockLevel(w_id, stockLevelDId, gen);
  } else {
  }
}

std::string TPCCClient::GetLastOp() const {
  return lastOp;
}


} //namespace tpcc
