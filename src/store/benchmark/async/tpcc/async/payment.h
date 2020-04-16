#ifndef ASYNC_PAYMENT_H
#define ASYNC_PAYMENT_H

#include <string>
#include <unordered_map>
#include <vector>
#include <random>

#include "store/benchmark/async/tpcc/payment.h"
#include "store/benchmark/async/tpcc/async/tpcc_transaction.h"
#include "store/benchmark/async/tpcc/tpcc-proto.pb.h"

namespace tpcc {

class AsyncPayment : public AsyncTPCCTransaction, public Payment {
 public:
  AsyncPayment(uint32_t w_id, uint32_t c_c_last, uint32_t c_c_id,
      uint32_t num_warehouses, std::mt19937 &gen);
  virtual ~AsyncPayment();

  Operation GetNextOperation(size_t opCount,
      std::map<std::string, std::string> readValues);

 private:
  WarehouseRow w_row;
  DistrictRow d_row;
  CustomerRow c_row;
  CustomerByNameRow cbn_row;
};

} // namespace tpcc

#endif /* ASYNC_PAYMENT_H */
