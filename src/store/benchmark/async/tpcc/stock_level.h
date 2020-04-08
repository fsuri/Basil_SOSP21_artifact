#ifndef STOCK_LEVEL_H
#define STOCK_LEVEL_H

#include <random>

#include "store/benchmark/async/tpcc/tpcc_transaction.h"

namespace tpcc {

class StockLevel : public TPCCTransaction {
 public:
  StockLevel(uint32_t w_id, uint32_t d_id, std::mt19937 &gen);
  virtual ~StockLevel();

 protected:
  uint32_t w_id;
  uint32_t d_id;
  uint8_t min_quantity;
};

} // namespace tpcc

#endif /* STOCK_LEVEL_H */
