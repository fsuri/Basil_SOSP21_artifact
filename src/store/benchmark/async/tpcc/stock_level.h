#ifndef STOCK_LEVEL_H
#define STOCK_LEVEL_H

#include <string>
#include <unordered_map>
#include <vector>
#include <random>

#include "store/benchmark/async/tpcc/tpcc_transaction.h"
#include "store/benchmark/async/tpcc/tpcc-proto.pb.h"

namespace tpcc {

class StockLevel : public TPCCTransaction {
 public:
  StockLevel(uint32_t w_id, uint32_t d_id, std::mt19937 &gen);
  virtual ~StockLevel();

  Operation GetNextOperation(size_t opCount,
      std::map<std::string, std::string> readValues);

 private:
  uint32_t w_id;
  uint32_t d_id;
  uint8_t min_quantity;
  uint32_t next_o_id;
  uint32_t currOrderIdx;
  uint32_t currOrderLineIdx;
  uint32_t readAllOrderLines;

  DistrictRow d_row;
  std::vector<OrderLineRow> orderLines;
};

} // namespace tpcc

#endif /* STOCK_LEVEL_H */
