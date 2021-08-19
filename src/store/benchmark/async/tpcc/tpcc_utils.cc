/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fsp@cs.cornell.edu>
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
#include "store/benchmark/async/tpcc/tpcc_utils.h"

#include "store/benchmark/async/tpcc/tpcc-proto.pb.h"

namespace tpcc {

const char* LAST_NAME_SYLLABLES[] = {
  "BAR",
  "OUGHT",
  "ABLE",
  "PRI",
  "PRES",
  "ESE",
  "ANTI",
  "CALLY",
  "ATION",
  "EING"
};

std::string GenerateCustomerLastName(int last) {
  std::string lastName =  LAST_NAME_SYLLABLES[last / 100];
  last = last % 100;
  lastName += LAST_NAME_SYLLABLES[last / 10];
  last = last % 10;
  lastName += LAST_NAME_SYLLABLES[last];
  return lastName;
}

std::string WarehouseRowKey(uint32_t w_id) {
  char keyC[5];
  keyC[0] = static_cast<char>(Tables::WAREHOUSE);
  *reinterpret_cast<uint32_t*>(keyC + 1) = w_id; 
  return std::string(keyC, sizeof(keyC));
}

std::string DistrictRowKey(uint32_t w_id, uint32_t d_id) {
  char keyC[9];
  keyC[0] = static_cast<char>(Tables::DISTRICT);
  *reinterpret_cast<uint32_t*>(keyC + 1) = w_id; 
  *reinterpret_cast<uint32_t*>(keyC + 5) = d_id; 
  return std::string(keyC, sizeof(keyC));
}

std::string CustomerRowKey(uint32_t w_id, uint32_t d_id, uint32_t c_id) {
  char keyC[13];
  keyC[0] = static_cast<char>(Tables::CUSTOMER);
  *reinterpret_cast<uint32_t*>(keyC + 1) = w_id; 
  *reinterpret_cast<uint32_t*>(keyC + 5) = d_id; 
  *reinterpret_cast<uint32_t*>(keyC + 9) = c_id; 
  return std::string(keyC, sizeof(keyC));
}

std::string NewOrderRowKey(uint32_t w_id, uint32_t d_id, uint32_t o_id) {
  char keyC[13];
  keyC[0] = static_cast<char>(Tables::NEW_ORDER);
  *reinterpret_cast<uint32_t*>(keyC + 1) = w_id; 
  *reinterpret_cast<uint32_t*>(keyC + 5) = d_id; 
  *reinterpret_cast<uint32_t*>(keyC + 9) = o_id; 
  return std::string(keyC, sizeof(keyC));
}

std::string OrderRowKey(uint32_t w_id, uint32_t d_id, uint32_t o_id) {
  char keyC[13];
  keyC[0] = static_cast<char>(Tables::ORDER);
  *reinterpret_cast<uint32_t*>(keyC + 1) = w_id; 
  *reinterpret_cast<uint32_t*>(keyC + 5) = d_id; 
  *reinterpret_cast<uint32_t*>(keyC + 9) = o_id; 
  return std::string(keyC, sizeof(keyC));
}

std::string OrderLineRowKey(uint32_t w_id, uint32_t d_id, uint32_t o_id, uint32_t ol_number) {
  char keyC[17];
  keyC[0] = static_cast<char>(Tables::ORDER_LINE);
  *reinterpret_cast<uint32_t*>(keyC + 1) = w_id; 
  *reinterpret_cast<uint32_t*>(keyC + 5) = d_id; 
  *reinterpret_cast<uint32_t*>(keyC + 9) = o_id; 
  *reinterpret_cast<uint32_t*>(keyC + 13) = ol_number; 
  return std::string(keyC, sizeof(keyC));
}

std::string ItemRowKey(uint32_t i_id) {
  char keyC[5];
  keyC[0] = static_cast<char>(Tables::ITEM);
  *reinterpret_cast<uint32_t*>(keyC + 1) = i_id; 
  return std::string(keyC, sizeof(keyC));
}

std::string StockRowKey(uint32_t w_id, uint32_t i_id) {
  char keyC[9];
  keyC[0] = static_cast<char>(Tables::STOCK);
  *reinterpret_cast<uint32_t*>(keyC + 1) = w_id; 
  *reinterpret_cast<uint32_t*>(keyC + 5) = i_id; 
  return std::string(keyC, sizeof(keyC));
}

std::string HistoryRowKey(uint32_t w_id, uint32_t d_id, uint32_t c_id) {
  char keyC[13];
  keyC[0] = static_cast<char>(Tables::HISTORY);
  *reinterpret_cast<uint32_t*>(keyC + 1) = w_id; 
  *reinterpret_cast<uint32_t*>(keyC + 5) = d_id; 
  *reinterpret_cast<uint32_t*>(keyC + 9) = c_id; 
  return std::string(keyC, sizeof(keyC));
}

std::string CustomerByNameRowKey(uint32_t w_id, uint32_t d_id, const std::string &c_last) {
  char keyC[9];
  keyC[0] = static_cast<char>(Tables::CUSTOMER_BY_NAME);
  *reinterpret_cast<uint32_t*>(keyC + 1) = w_id; 
  *reinterpret_cast<uint32_t*>(keyC + 5) = d_id; 
  return std::string(keyC, sizeof(keyC)) + c_last;
}

std::string OrderByCustomerRowKey(uint32_t w_id, uint32_t d_id, uint32_t c_id) {
  char keyC[13];
  keyC[0] = static_cast<char>(Tables::ORDER_BY_CUSTOMER);
  *reinterpret_cast<uint32_t*>(keyC + 1) = w_id; 
  *reinterpret_cast<uint32_t*>(keyC + 5) = d_id; 
  *reinterpret_cast<uint32_t*>(keyC + 9) = c_id; 
  return std::string(keyC, sizeof(keyC));
}

std::string EarliestNewOrderRowKey(uint32_t w_id, uint32_t d_id) {
  char keyC[9];
  keyC[0] = static_cast<char>(Tables::EARLIEST_NEW_ORDER);
  *reinterpret_cast<uint32_t*>(keyC + 1) = w_id; 
  *reinterpret_cast<uint32_t*>(keyC + 5) = d_id; 
  return std::string(keyC, sizeof(keyC));
}

}
