#include "store/benchmark/async/tpcc/stock_level.h"

#include <chrono>
#include <sstream>
#include <ctime>

#include "store/benchmark/async/tpcc/limits.h"
#include "store/benchmark/async/tpcc/tables.h"
#include "store/benchmark/async/tpcc/tpcc-proto.pb.h"
#include "store/benchmark/async/tpcc/tpcc_utils.h"

namespace tpcc {

StockLevel::StockLevel(uint32_t w_id, uint32_t d_id, std::mt19937 &gen) :
    w_id(w_id), d_id(d_id) {
  min_quantity = std::uniform_int_distribution<uint8_t>(10, 20)(gen);
}

StockLevel::~StockLevel() {
}

Operation StockLevel::GetNextOperation(size_t opCount,
  std::map<std::string, std::string> readValues) {
  if (opCount == 0) {
    return Get(DistrictRowKey(w_id, d_id));
  } else {
    if (opCount == 1) {
      std::string d_key = DistrictRowKey(w_id, d_id);
      auto d_row_itr = readValues.find(d_key);
      ASSERT(d_row_itr != readValues.end());
      ASSERT(d_row.ParseFromString(d_row_itr->second));

      next_o_id = d_row.next_o_id();
    }
  }
}

}
