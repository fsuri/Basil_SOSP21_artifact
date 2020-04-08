#include "store/benchmark/async/tpcc/sync/payment.h"

namespace tpcc {

SyncPayment::SyncPayment(uint32_t w_id, uint32_t c_c_last, uint32_t c_c_id,
      uint32_t num_warehouses, std::mt19937 &gen) : Payment(w_id, c_c_last,
        c_c_id, num_warehouses, gen) {
}

SyncPayment::~SyncPayment() {
}

int SyncPayment::Execute(SyncClient &client) {
  return 0;
}

} // namespace tpcc
