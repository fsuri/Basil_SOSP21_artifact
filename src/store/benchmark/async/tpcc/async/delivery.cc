#include "store/benchmark/async/tpcc/async/delivery.h"

#include <chrono>
#include <sstream>
#include <ctime>

#include "store/benchmark/async/tpcc/tpcc-proto.pb.h"
#include "store/benchmark/async/tpcc/tpcc_utils.h"

namespace tpcc {

AsyncDelivery::AsyncDelivery(uint32_t w_id, uint32_t d_id, std::mt19937 &gen) :
    Delivery(w_id, d_id, gen) {
}

AsyncDelivery::~AsyncDelivery() {
}

Operation AsyncDelivery::GetNextOperation(size_t opCount,
  std::map<std::string, std::string> readValues) {
  // if done with customer/orderline/order/neworder in curr d
  //    increment curr d
  // if curr d > 10
  //    commit
  // else
  switch (state) {
    case DS_EARLIEST_NO:
      state = DS_NEW_ORDER;
      return Get(EarliestNewOrderRowKey(w_id, d_id)); 
    case DS_NEW_ORDER:
      std::string eno_key = EarliestNewOrderRowKey(w_id, d_id);
      auto eno_row_itr = readValues.find(eno_key);
      UW_ASSERT(eno_row_itr != readValues.end());
      UW_ASSERT(eno_row.ParseFromString(eno_row_itr->second));

      o_id = eno_row.o_id();
      state = DS_ORDER;
      return Get(NewOrderRowKey(w_id, d_id, o_id));
    case DS_ORDER:
      return Get(OrderRowKey(w_id, d_id, o_id));
    case DS_ORDER_LINES:
    case DS_CUSTOMER:
    default:
      NOT_REACHABLE();

  }
}

}
