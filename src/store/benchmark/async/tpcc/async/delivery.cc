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

Operation AsyncDelivery::GetNextOperation(size_t outstandingOpCount, size_t finishedOpCount,
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
