#include "store/benchmark/async/tpcc/stock_level.h"

#include <chrono>
#include <sstream>
#include <ctime>

#include "store/benchmark/async/tpcc/tpcc-proto.pb.h"
#include "store/benchmark/async/tpcc/tpcc_utils.h"

namespace tpcc {

StockLevel::StockLevel(uint32_t w_id, uint32_t d_id, std::mt19937 &gen) :
    w_id(w_id), d_id(d_id), currOrderIdx(0UL), currOrderLineIdx(0UL),
    readAllOrderLines(0UL) {
  min_quantity = std::uniform_int_distribution<uint8_t>(10, 20)(gen);
}

StockLevel::~StockLevel() {
}

Operation StockLevel::GetNextOperation(size_t opCount,
  std::map<std::string, std::string> readValues) {
  if (opCount == 0) {
    Debug("STOCK_LEVEL");
    Debug("Warehouse: %u", w_id);
    Debug("District: %u", d_id);
    return Get(DistrictRowKey(w_id, d_id));
  } else if (readAllOrderLines == 0) {
    if (opCount == 1) {
      std::string d_key = DistrictRowKey(w_id, d_id);
      auto d_row_itr = readValues.find(d_key);
      UW_ASSERT(d_row_itr != readValues.end());
      UW_ASSERT(d_row.ParseFromString(d_row_itr->second));

      next_o_id = d_row.next_o_id();
      Debug("Orders: %u-%u", next_o_id - 20, next_o_id - 1);
      Debug("Order %u", next_o_id - 20);
    }

    uint32_t prev_ol_o_id = next_o_id - 20 + currOrderIdx;
    std::string prev_ol_key = OrderLineRowKey(w_id, d_id, prev_ol_o_id,
        currOrderLineIdx - 1);
    auto prev_ol_value_itr = readValues.find(prev_ol_key);
    if (currOrderLineIdx != 0) {
      UW_ASSERT(prev_ol_value_itr != readValues.end());
      if (prev_ol_value_itr->second.empty()) {
        // order_line was not found
        ++currOrderIdx;
        Debug("Order %u", next_o_id - 20 + currOrderIdx);
        currOrderLineIdx = 0;
      } else {
        OrderLineRow ol_row;
        UW_ASSERT(ol_row.ParseFromString(prev_ol_value_itr->second));
        orderLines.push_back(ol_row);
        Debug("  Order Line %u", currOrderLineIdx);
        Debug("    Item: %u", ol_row.i_id());
      }
    }
    uint32_t ol_o_id = next_o_id - 20 + currOrderIdx;
    if (ol_o_id < next_o_id) {
      std::string ol_key = OrderLineRowKey(w_id, d_id, ol_o_id, currOrderLineIdx);
      ++currOrderLineIdx;
      return Get(ol_key);
    } else {
      readAllOrderLines = opCount;
    }
  }
  UW_ASSERT(readAllOrderLines > 0);
  uint32_t orderLineIdx = opCount - readAllOrderLines;
  if (orderLineIdx < orderLines.size()) {
    return Get(StockRowKey(w_id, orderLines[orderLineIdx].i_id()));
  } else {
    return Commit();
  }
}


}
