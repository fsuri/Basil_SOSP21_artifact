/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fs435@cornell.edu>
 *                Matthew Burke <matthelb@cs.cornell.edu>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************************/
#include "store/benchmark/async/tpcc/async/stock_level.h"

#include <chrono>
#include <sstream>
#include <ctime>

#include "store/benchmark/async/tpcc/tpcc-proto.pb.h"
#include "store/benchmark/async/tpcc/tpcc_utils.h"

namespace tpcc {

AsyncStockLevel::AsyncStockLevel(uint32_t w_id, uint32_t d_id,
    std::mt19937 &gen) : StockLevel(w_id, d_id, gen),
    currOrderIdx(0UL), currOrderLineIdx(0UL), readAllOrderLines(0UL) {
}

AsyncStockLevel::~AsyncStockLevel() {
}

Operation AsyncStockLevel::GetNextOperation(size_t outstandingOpCount, size_t finishedOpCount,
  std::map<std::string, std::string> readValues) {
  if (finishedOpCount == 0) {
    Debug("STOCK_LEVEL");
    Debug("Warehouse: %u", w_id);
    Debug("District: %u", d_id);
    return Get(DistrictRowKey(w_id, d_id));
  } else if (readAllOrderLines == 0) {
    if (finishedOpCount == 1) {
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
      readAllOrderLines = finishedOpCount;
    }
  }
  UW_ASSERT(readAllOrderLines > 0);
  uint32_t orderLineIdx = finishedOpCount - readAllOrderLines;
  if (orderLineIdx < orderLines.size()) {
    return Get(StockRowKey(w_id, orderLines[orderLineIdx].i_id()));
  } else if (orderLineIdx == orderLines.size()) {
    return Commit();
  } else {
    return Wait();
  }
}


}
