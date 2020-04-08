#include "store/benchmark/async/tpcc/sync/order_status.h"

namespace tpcc {

SyncOrderStatus::SyncOrderStatus(uint32_t w_id, uint32_t c_c_last,
    uint32_t c_c_id, std::mt19937 &gen) : OrderStatus(w_id, c_c_last,
      c_c_id, gen) {
}

SyncOrderStatus::~SyncOrderStatus() {
}

int SyncOrderStatus::Execute(SyncClient &client) {
  return 0;
}

} // namespace tpcc
