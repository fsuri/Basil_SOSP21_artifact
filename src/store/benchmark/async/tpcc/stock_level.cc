#include "store/benchmark/async/tpcc/stock_level.h"

#include <chrono>
#include <sstream>
#include <ctime>

#include "store/benchmark/async/tpcc/tpcc_utils.h"

namespace tpcc {

StockLevel::StockLevel(uint32_t w_id, uint32_t d_id, std::mt19937 &gen) :
    w_id(w_id), d_id(d_id) {
  min_quantity = std::uniform_int_distribution<uint8_t>(10, 20)(gen);
}``

StockLevel::~StockLevel() {
}

}
