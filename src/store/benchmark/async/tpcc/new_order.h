#ifndef NEW_ORDER_H
#define NEW_ORDER_H

#include <vector>
#include <random>

#include "store/benchmark/async/tpcc/tpcc_transaction.h"

namespace tpcc {

class NewOrder : public TPCCTransaction {
 public:
  NewOrder(uint32_t w_id, uint32_t C, uint32_t num_warehouses, std::mt19937 &gen);
  virtual ~NewOrder();

 protected:
  uint32_t w_id;
  uint32_t d_id;
  uint32_t c_id;
  uint8_t ol_cnt;
  uint8_t rbk;
  std::vector<uint32_t> o_ol_i_ids;
  std::vector<uint32_t> o_ol_supply_w_ids;
  std::vector<uint8_t> o_ol_quantities;
  uint32_t o_entry_d;
  bool all_local;
};

}

#endif /* NEW_ORDER_H */
