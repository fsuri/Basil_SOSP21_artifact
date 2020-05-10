#ifndef ASYNC_STOCK_LEVEL_H
#define ASYNC_STOCK_LEVEL_H

#include <string>
#include <unordered_map>
#include <vector>
#include <random>

#include "store/benchmark/async/tpcc/stock_level.h"
#include "store/benchmark/async/tpcc/async/tpcc_transaction.h"
#include "store/benchmark/async/tpcc/tpcc-proto.pb.h"

namespace tpcc {

class AsyncStockLevel : public AsyncTPCCTransaction, public StockLevel {
 public:
  AsyncStockLevel(uint32_t w_id, uint32_t d_id, std::mt19937 &gen);
  virtual ~AsyncStockLevel();

  Operation GetNextOperation(size_t outstandingOpCount, size_t finishedOpCount,
      std::map<std::string, std::string> readValues);

 protected:
  uint32_t next_o_id;
  uint32_t currOrderIdx;
  uint32_t currOrderLineIdx;
  uint32_t readAllOrderLines;
  DistrictRow d_row;
  std::vector<OrderLineRow> orderLines;
};

} // namespace tpcc

#endif /* ASYNC_STOCK_LEVEL_H */
