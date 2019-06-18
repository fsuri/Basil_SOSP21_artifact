#ifndef PAYMENT_H
#define PAYMENT_H

#include <string>
#include <unordered_map>
#include <vector>
#include <random>

#include "store/benchmark/async/tpcc/tpcc_transaction.h"
#include "store/benchmark/async/tpcc/tpcc-proto.pb.h"

namespace tpcc {

class Payment : public TPCCTransaction {
 public:
  Payment(uint32_t w_id, uint32_t c_c_last, uint32_t c_c_id, uint32_t num_warehouses, std::mt19937 &gen);
  virtual ~Payment();

  Operation GetNextOperation(size_t opCount,
      std::map<std::string, std::string> readValues);

 private:
  uint32_t w_id;
  uint32_t d_id;
  uint32_t c_w_id;
  uint32_t c_d_id;
  uint32_t c_id;
  uint32_t h_amount;
  uint32_t h_date;
  bool c_by_last_name;
  std::string c_last;

  WarehouseRow w_row;
  DistrictRow d_row;
  CustomerRow c_row;
  CustomerByNameRow cbn_row;
};

}

#endif /* PAYMENT_H */
