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
#ifndef ASYNC_STOCK_LEVEL_H
#define ASYNC_STOCK_LEVEL_H

#include <string>
#include <unordered_map>
#include <vector>
#include <random>

#include "store/benchmark/async/tpcc/stock_level.h"
#include "store/benchmark/async/tpcc/async/tpcc_transaction.h"
#include "store/benchmark/async/tpcc/tpcc-proto.pb.h"

namespace tpcc {

class AsyncStockLevel : public AsyncTPCCTransaction, public StockLevel {
 public:
  AsyncStockLevel(uint32_t w_id, uint32_t d_id, std::mt19937 &gen);
  virtual ~AsyncStockLevel();

  Operation GetNextOperation(size_t outstandingOpCount, size_t finishedOpCount,
      std::map<std::string, std::string> readValues);

 protected:
  uint32_t next_o_id;
  uint32_t currOrderIdx;
  uint32_t currOrderLineIdx;
  uint32_t readAllOrderLines;
  DistrictRow d_row;
  std::vector<OrderLineRow> orderLines;
};

} // namespace tpcc

#endif /* ASYNC_STOCK_LEVEL_H */
