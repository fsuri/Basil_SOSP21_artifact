#ifndef SYNC_ORDER_STATUS_H
#define SYNC_ORDER_STATUS_H

#include "store/benchmark/async/tpcc/sync/tpcc_transaction.h"
#include "store/benchmark/async/tpcc/order_status.h"

namespace tpcc {

class SyncOrderStatus : public SyncTPCCTransaction, public OrderStatus {
 public:
  SyncOrderStatus(uint32_t w_id, uint32_t c_c_last, uint32_t c_c_id,
      std::mt19937 &gen);
  virtual ~SyncOrderStatus();
  virtual int Execute(SyncClient &client);

};

} // namespace tpcc

#endif /* SYNC_ORDER_STATUS_H */
