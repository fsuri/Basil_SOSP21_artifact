#ifndef ORDER_STATUS_H
#define ORDER_STATUS_H

#include <string>
#include <unordered_map>
#include <vector>
#include <random>

#include "store/benchmark/async/tpcc/tpcc_transaction.h"
#include "store/benchmark/async/tpcc/tpcc-proto.pb.h"

namespace tpcc {

class OrderStatus : public TPCCTransaction {
 public:
  OrderStatus(uint32_t w_id, uint32_t c_c_last, uint32_t c_c_id, std::mt19937 &gen);
  virtual ~OrderStatus();

  Operation GetNextOperation(size_t opCount,
      std::map<std::string, std::string> readValues);

 private:
  uint32_t w_id;
  uint32_t d_id;
  uint32_t c_w_id;
  uint32_t c_d_id;
  uint32_t c_id;
  uint32_t o_id;
  bool c_by_last_name;
  std::string c_last;

  CustomerRow c_row;
  CustomerByNameRow cbn_row;
  OrderByCustomerRow obc_row;
  OrderRow o_row;
};

} // namespace tpcc

#endif /* ORDER_STATUS_H */
