#include "store/benchmark/async/tpcc/delivery.h"

#include <chrono>
#include <sstream>
#include <ctime>

#include "store/benchmark/async/tpcc/tpcc-proto.pb.h"
#include "store/benchmark/async/tpcc/tpcc_utils.h"

namespace tpcc {

Delivery::Delivery(uint32_t w_id, std::mt19937 &gen) : w_id(w_id), currDId(1U) {
  o_carrier_id = std::uniform_int_distribution<uint32_t>(1, 10)(gen);
  ol_delivery_d = std::time(0);
}

Delivery::~Delivery() {
}

Operation Delivery::GetNextOperation(size_t opCount,
  std::map<std::string, std::string> readValues) {
  // if done with customer/orderline/order/neworder in curr d
  //    increment curr d
  // if curr d > 10
  //    commit
  // else
  switch (state) {
    case EARLIEST_NO:
      state = NEW_ORDER;
      return Get(EarliestNewOrderRowKey(w_id, d_id)); 
    case NEW_ORDER:
      std::string eno_key = EarliestNewOrderRowKey(w_id, d_id);
      auto eno_row_itr = readValues.find(eno_key);
      UW_ASSERT(eno_row_itr != readValues.end());
      UW_ASSERT(eno_row.ParseFromString(eno_row_itr->second));

      o_id = eno_row.o_id();
      state = ORDER;
      return Get(NewOrderRowKey(w_id, d_id, o_id));
    case ORDER:
      return Get(OrderRowKey(w_id, d_id, o_id));
    case ORDER_LINES:
    case CUSTOMER:
    default:
      NOT_REACHABLE();

  }
}

}
