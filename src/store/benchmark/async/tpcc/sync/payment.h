#ifndef SYNC_PAYMENT_H
#define SYNC_PAYMENT_H

#include "store/benchmark/async/tpcc/sync/tpcc_transaction.h"
#include "store/benchmark/async/tpcc/payment.h"

namespace tpcc {

class SyncPayment : public SyncTPCCTransaction, public Payment {
 public:
  SyncPayment(uint32_t timeout, uint32_t w_id, uint32_t c_c_last,
      uint32_t c_c_id, uint32_t num_warehouses, std::mt19937 &gen);
  virtual ~SyncPayment();
  virtual int Execute(SyncClient &client);

};

} // namespace tpcc

#endif /* SYNC_PAYMENT_H */
