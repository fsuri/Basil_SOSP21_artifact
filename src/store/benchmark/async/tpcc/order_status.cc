#include "store/benchmark/async/tpcc/order_status.h"

#include <chrono>
#include <sstream>
#include <ctime>

#include "store/benchmark/async/tpcc/tpcc_utils.h"

namespace tpcc {

OrderStatus::OrderStatus(uint32_t w_id, uint32_t c_c_last, uint32_t c_c_id,
    std::mt19937 &gen) : w_id(w_id) {
  d_id = std::uniform_int_distribution<uint32_t>(1, 10)(gen); 
  int y = std::uniform_int_distribution<int>(1, 100)(gen);
  c_w_id = w_id;
  c_d_id = d_id;
  if (y <= 60) {
    int last = NURand(255, 0, 999, static_cast<int>(c_c_last), gen);
    c_last = GenerateCustomerLastName(last);
    c_by_last_name = true;
  } else {
    c_id = NURand(1023, 1, 3000, static_cast<int>(c_c_id), gen);
    c_by_last_name = false;
  }
}

OrderStatus::~OrderStatus() {
}


}
