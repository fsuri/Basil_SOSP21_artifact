#include "store/benchmark/async/tpcc/payment.h"

#include <chrono>
#include <sstream>
#include <ctime>

#include "store/benchmark/async/tpcc/limits.h"
#include "store/benchmark/async/tpcc/tables.h"
#include "store/benchmark/async/tpcc/tpcc-proto.pb.h"
#include "store/benchmark/async/tpcc/tpcc_utils.h"

namespace tpcc {

Payment::Payment(uint32_t w_id, uint32_t c_c_last, uint32_t c_c_id, uint32_t num_warehouses, std::mt19937 &gen) : w_id(w_id) {
  d_id = std::uniform_int_distribution<uint32_t>(1, 10)(gen); 
  int x = std::uniform_int_distribution<int>(1, 100)(gen);
  int y = std::uniform_int_distribution<int>(1, 100)(gen);
  if (x <= 85) {
    c_w_id = w_id;
    c_d_id = d_id;
  } else {
    c_w_id = std::uniform_int_distribution<uint32_t>(1, num_warehouses - 1)(gen);
    if (c_w_id == w_id) {
      c_w_id = num_warehouses; // simple swap to ensure uniform distribution
    }
    c_d_id = std::uniform_int_distribution<uint32_t>(1, 10)(gen);
  }
  if (y <= 60) {
    int last = NURand(255, 0, 999, c_c_last, gen);
    c_last = GenerateCustomerLastName(last);
  } else {
    c_id = NURand(1023, 1, 3000, c_c_id, gen);
  }
  h_amount = std::uniform_int_distribution<uint32_t>(100, 500000)(gen);
  h_date = std::time(0);
}

Payment::~Payment() {
}

Operation Payment::GetNextOperation(size_t opCount,
  std::map<std::string, std::string> readValues) {
}

