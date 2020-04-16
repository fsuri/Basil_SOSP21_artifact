#include "store/benchmark/async/tpcc/new_order.h"

#include <chrono>
#include <sstream>
#include <ctime>

#include "store/benchmark/async/tpcc/tpcc-proto.pb.h"
#include "store/benchmark/async/tpcc/tpcc_utils.h"

namespace tpcc {

NewOrder::NewOrder(uint32_t w_id, uint32_t C, uint32_t num_warehouses,
    std::mt19937 &gen) : w_id(w_id) {
  d_id = std::uniform_int_distribution<uint32_t>(1, 10)(gen); 
  c_id = NURand(static_cast<uint32_t>(1023), static_cast<uint32_t>(1), static_cast<uint32_t>(3000), C, gen);
  ol_cnt = std::uniform_int_distribution<uint8_t>(5, 15)(gen);
  rbk = std::uniform_int_distribution<uint8_t>(1, 100)(gen);
  all_local = true;
  for (uint8_t i = 0; i < ol_cnt; ++i) {
    if (rbk == 1 && i == ol_cnt - 1) {
      o_ol_i_ids.push_back(0);
    } else {
      o_ol_i_ids.push_back(NURand(static_cast<uint32_t>(8191), static_cast<uint32_t>(1), static_cast<uint32_t>(100000), C, gen));
    }
    uint8_t x = std::uniform_int_distribution<uint8_t>(1, 100)(gen);
    if (x == 1 && num_warehouses > 1) {
      uint32_t remote_w_id = std::uniform_int_distribution<uint32_t>(1, num_warehouses - 1)(gen);
      if (remote_w_id == w_id) {
        remote_w_id = num_warehouses; // simple swap to ensure uniform distribution
      }
      o_ol_supply_w_ids.push_back(remote_w_id);
      all_local = false;
    } else {
      o_ol_supply_w_ids.push_back(w_id);
    }
    o_ol_quantities.push_back(std::uniform_int_distribution<uint8_t>(1, 10)(gen));
  }
  o_entry_d = std::time(0);
}

NewOrder::~NewOrder() {
}

}
