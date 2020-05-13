#ifndef ASYNC_ORDER_STATUS_H
#define ASYNC_ORDER_STATUS_H

#include <string>
#include <unordered_map>
#include <vector>
#include <random>

#include "store/benchmark/async/tpcc/order_status.h"
#include "store/benchmark/async/tpcc/async/tpcc_transaction.h"
#include "store/benchmark/async/tpcc/tpcc-proto.pb.h"

namespace tpcc {

class AsyncOrderStatus : public AsyncTPCCTransaction, public OrderStatus {
 public:
  AsyncOrderStatus(uint32_t w_id, uint32_t c_c_last, uint32_t c_c_id,
      std::mt19937 &gen);
  virtual ~AsyncOrderStatus();

  Operation GetNextOperation(size_t outstandingOpCount, size_t finishedOpCount,
      std::map<std::string, std::string> readValues);

 private:
  CustomerRow c_row;
  CustomerByNameRow cbn_row;
  OrderByCustomerRow obc_row;
  OrderRow o_row;
};

} // namespace tpcc

#endif /* ASYNC_ORDER_STATUS_H */
