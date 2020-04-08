#include "store/benchmark/async/tpcc/sync/delivery.h"

namespace tpcc {

SyncDelivery::SyncDelivery(uint32_t w_id, uint32_t d_id, std::mt19937 &gen)
    : Delivery(w_id, d_id, gen) {
}

SyncDelivery::~SyncDelivery() {
}

int SyncDelivery::Execute(SyncClient &client) {
  return 0;
}

} // namespace tpcc
