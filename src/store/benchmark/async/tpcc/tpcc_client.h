#ifndef TPCC_CLIENT_H
#define TPCC_CLIENT_H

#include <random>

#include "store/benchmark/async/async_transaction_bench_client.h"

namespace tpcc {

enum TPCCTransactionType {
  TXN_NEW_ORDER = 0,
  TXN_PAYMENT,
  TXN_ORDER_STATUS,
  TXN_STOCK_LEVEL,
  TXN_DELIVERY,
  NUM_TXN_TYPES
};

class TPCCClient {
 public:
  TPCCClient(uint32_t num_warehouses, uint32_t w_id,
      uint32_t C_c_id, uint32_t C_c_last, uint32_t new_order_ratio,
      uint32_t delivery_ratio, uint32_t payment_ratio, uint32_t order_status_ratio,
      uint32_t stock_level_ratio, bool static_w_id, std::mt19937 &gen);

  virtual ~TPCCClient();

 protected:
  virtual TPCCTransactionType GetNextTransaction(uint32_t *wid, uint32_t *did,
      std::mt19937& gen);
  std::string GetLastTransaction() const;

  uint32_t num_warehouses;
  uint32_t w_id;
  uint32_t C_c_id;
  uint32_t C_c_last;
  uint32_t new_order_ratio;
  uint32_t delivery_ratio;
  uint32_t payment_ratio;
  uint32_t order_status_ratio;
  uint32_t stock_level_ratio;
  bool static_w_id;
  uint32_t stockLevelDId;
  std::string lastOp;

 private:
  bool delivery;
  uint32_t deliveryWId;
  uint32_t deliveryDId;

};

} //namespace tpcc

#endif /* TPCC_CLIENT_H */
