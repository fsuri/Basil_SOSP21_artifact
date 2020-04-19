#ifndef SYNC_DELIVERY_H
#define SYNC_DELIVERY_H

#include "store/benchmark/async/tpcc/sync/tpcc_transaction.h"
#include "store/benchmark/async/tpcc/delivery.h"

namespace tpcc {

class SyncDelivery : public SyncTPCCTransaction, public Delivery {
 public:
  SyncDelivery(uint32_t timeout, uint32_t w_id, uint32_t d_id,
      std::mt19937 &gen);
  virtual ~SyncDelivery();
  virtual int Execute(SyncClient &client);

};

} // namespace tpcc

#endif /* SYNC_DELIVERY_H */
