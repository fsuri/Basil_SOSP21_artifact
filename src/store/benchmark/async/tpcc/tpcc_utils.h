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
#ifndef TPCC_UTILS_H
#define TPCC_UTILS_H

#include <random>

namespace tpcc {

std::string GenerateCustomerLastName(int last);

std::string WarehouseRowKey(uint32_t w_id);
std::string DistrictRowKey(uint32_t w_id, uint32_t d_id);
std::string CustomerRowKey(uint32_t w_id, uint32_t d_id, uint32_t c_id);
std::string NewOrderRowKey(uint32_t w_id, uint32_t d_id, uint32_t o_id);
std::string OrderRowKey(uint32_t w_id, uint32_t d_id, uint32_t o_id);
std::string OrderLineRowKey(uint32_t w_id, uint32_t d_id, uint32_t o_id, uint32_t ol_number);
std::string ItemRowKey(uint32_t i_id);
std::string StockRowKey(uint32_t w_id, uint32_t i_id);
std::string HistoryRowKey(uint32_t w_id, uint32_t d_id, uint32_t c_id);
std::string CustomerByNameRowKey(uint32_t w_id, uint32_t d_id, const std::string &c_last);
std::string OrderByCustomerRowKey(uint32_t w_id, uint32_t d_id, uint32_t c_id);
std::string EarliestNewOrderRowKey(uint32_t w_id, uint32_t d_id);

template<class T>
uint32_t NURand(T A, T x, T y, T C, std::mt19937 &gen) {
  std::uniform_int_distribution<T> d1(0, A);
  std::uniform_int_distribution<T> d2(x, y);
  return (((d1(gen) | d2(gen)) + C) % (y - x + 1)) + x;
}

} // namespace tpcc

#endif /* TPCC_UTILS_H */
