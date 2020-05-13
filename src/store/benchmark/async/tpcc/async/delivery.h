#ifndef ASYNC_DELIVERY_H
#define ASYNC_DELIVERY_H

#include <string>
#include <unordered_map>
#include <vector>
#include <random>

#include "store/benchmark/async/tpcc/delivery.h"
#include "store/benchmark/async/tpcc/async/tpcc_transaction.h"

namespace tpcc {

enum DeliveryState {
  DS_EARLIEST_NO = 0,
  DS_NEW_ORDER,
  DS_ORDER,
  DS_ORDER_LINES,
  DS_CUSTOMER
};

class AsyncDelivery : public AsyncTPCCTransaction, public Delivery {
 public:
  AsyncDelivery(uint32_t w_id, uint32_t d_id, std::mt19937 &gen);
  virtual ~AsyncDelivery();

  Operation GetNextOperation(size_t outstandingOpCount, size_t finishedOpCount,
      std::map<std::string, std::string> readValues);

 private:
  uint8_t currDId;
  DeliveryState state;
};

} // namespace tpcc

#endif /* ASYNC_DELIVERY_H */
