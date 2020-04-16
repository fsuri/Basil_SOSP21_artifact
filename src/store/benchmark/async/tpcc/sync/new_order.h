#ifndef SYNC_NEW_ORDER_H
#define SYNC_NEW_ORDER_H

#include "store/benchmark/async/tpcc/sync/tpcc_transaction.h"
#include "store/benchmark/async/tpcc/new_order.h"

namespace tpcc {

class SyncNewOrder : public SyncTPCCTransaction, public NewOrder {
 public:
  SyncNewOrder(uint32_t w_id, uint32_t C, uint32_t num_warehouses,
      std::mt19937 &gen);
  virtual ~SyncNewOrder();
  virtual int Execute(SyncClient &client);

};

} // namespace tpcc

#endif /* SYNC_NEW_ORDER_H */
