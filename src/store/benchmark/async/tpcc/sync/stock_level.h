#ifndef SYNC_STOCK_LEVEL_H
#define SYNC_STOCK_LEVEL_H

#include "store/benchmark/async/tpcc/sync/tpcc_transaction.h"
#include "store/benchmark/async/tpcc/stock_level.h"

namespace tpcc {

class SyncStockLevel : public SyncTPCCTransaction, public StockLevel {
 public:
  SyncStockLevel(uint32_t w_id, uint32_t d_id, std::mt19937 &gen);
  virtual ~SyncStockLevel();
  virtual int Execute(SyncClient &client);

};

} // namespace tpcc

#endif /* SYNC_STOCK_LEVEL_H */
