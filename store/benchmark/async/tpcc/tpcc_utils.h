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

template<class T>
uint32_t NURand(T A, T x, T y, T C, std::mt19937 &gen) {
  std::uniform_int_distribution<T> d1(0, A);
  std::uniform_int_distribution<T> d2(x, y);
  return (((d1(gen) | d2(gen)) + C) % (y - x + 1)) + x;
}

} // namespace tpcc

#endif /* TPCC_UTILS_H */
