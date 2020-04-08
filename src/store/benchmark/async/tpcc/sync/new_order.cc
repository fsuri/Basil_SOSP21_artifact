#include "store/benchmark/async/tpcc/sync/new_order.h"

namespace tpcc {

SyncNewOrder::SyncNewOrder(uint32_t w_id, uint32_t C, uint32_t num_warehouses,
      std::mt19937 &gen) : NewOrder(w_id, C, num_warehouses, gen) {
}

SyncNewOrder::~SyncNewOrder() {
}

int SyncNewOrder::Execute(SyncClient &client) {
  return 0;
}

} // namespace tpcc
