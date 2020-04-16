#ifndef DELIVERY_H
#define DELIVERY_H

#include <string>
#include <unordered_map>
#include <vector>
#include <random>

#include "store/benchmark/async/tpcc/tpcc_transaction.h"

namespace tpcc {

class Delivery : public TPCCTransaction {
 public:
  Delivery(uint32_t w_id, uint32_t d_id, std::mt19937 &gen);
  virtual ~Delivery();

 protected:
  uint32_t w_id;
  uint32_t d_id;
  uint32_t o_carrier_id;
  uint32_t ol_delivery_d;
};

} // namespace tpcc

#endif /* DELIVERY_H */
