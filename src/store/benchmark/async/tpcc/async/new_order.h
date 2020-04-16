#ifndef ASYNC_NEW_ORDER_H
#define ASYNC_NEW_ORDER_H

#include <string>
#include <unordered_map>
#include <vector>
#include <random>

#include "store/benchmark/async/tpcc/new_order.h"
#include "store/benchmark/async/tpcc/async/tpcc_transaction.h"
#include "store/benchmark/async/tpcc/tpcc-proto.pb.h"

namespace tpcc {

class AsyncNewOrder : public AsyncTPCCTransaction, public NewOrder {
 public:
  AsyncNewOrder(uint32_t w_id, uint32_t C, uint32_t num_warehouses,
      std::mt19937 &gen);
  virtual ~AsyncNewOrder();

  Operation GetNextOperation(size_t opCount,
      std::map<std::string, std::string> readValues);

 protected:
  uint32_t o_id;

  DistrictRow d_row;
  StockRow *s_row;
  ItemRow *i_row;
};

}

#endif /* ASYNC_NEW_ORDER_H */
