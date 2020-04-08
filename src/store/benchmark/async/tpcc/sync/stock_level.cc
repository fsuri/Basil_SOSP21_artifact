#include "store/benchmark/async/tpcc/sync/stock_level.h"

namespace tpcc {

SyncStockLevel::SyncStockLevel(uint32_t w_id, uint32_t d_id, std::mt19937 &gen)
    : StockLevel(w_id, d_id, gen) {
}

SyncStockLevel::~SyncStockLevel() {
}

int SyncStockLevel::Execute(SyncClient &client) {
  return 0;
}

} // namespace tpcc
