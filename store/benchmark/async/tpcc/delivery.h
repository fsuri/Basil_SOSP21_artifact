#ifndef DELIVERY_H
#define DELIVERY_H

#include <string>
#include <unordered_map>
#include <vector>
#include <random>

#include "store/benchmark/async/tpcc/tpcc_transaction.h"
#include "store/benchmark/async/tpcc/tpcc-proto.pb.h"

namespace tpcc {

enum DeliveryState {
  EARLIEST_NO = 0,
  NEW_ORDER,
  ORDER,
  ORDER_LINES,
  CUSTOMER
};

class Delivery : public TPCCTransaction {
 public:
  Delivery(uint32_t w_id, std::mt19937 &gen);
  virtual ~Delivery();

  Operation GetNextOperation(size_t opCount,
      std::map<std::string, std::string> readValues);

 private:
  uint32_t w_id;
  uint32_t o_carrier_id;
  uint32_t ol_delivery_d;

  uint8_t currDId;
  DeliveryState state;
};

} // namespace tpcc

#endif /* DELIVERY_H */
