#include "store/benchmark/async/tpcc/delivery.h"

#include <chrono>
#include <sstream>
#include <ctime>

#include "store/benchmark/async/tpcc/tpcc_utils.h"

namespace tpcc {

Delivery::Delivery(uint32_t w_id, uint32_t d_id, std::mt19937 &gen) : w_id(w_id),
    d_id(d_id) {
  o_carrier_id = std::uniform_int_distribution<uint32_t>(1, 10)(gen);
  ol_delivery_d = std::time(0);
}

Delivery::~Delivery() {
}

}
