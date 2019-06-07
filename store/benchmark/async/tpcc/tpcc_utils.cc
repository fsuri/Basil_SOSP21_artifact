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
  return keyC;
}

std::string DistrictRowKey(uint32_t w_id, uint32_t d_id) {
  char keyC[9];
  keyC[0] = static_cast<char>(Tables::DISTRICT);
  *reinterpret_cast<uint32_t*>(keyC + 1) = w_id; 
  *reinterpret_cast<uint32_t*>(keyC + 5) = d_id; 
  return keyC;
}

std::string CustomerRowKey(uint32_t w_id, uint32_t d_id, uint32_t c_id) {
  char keyC[13];
  keyC[0] = static_cast<char>(Tables::CUSTOMER);
  *reinterpret_cast<uint32_t*>(keyC + 1) = w_id; 
  *reinterpret_cast<uint32_t*>(keyC + 5) = d_id; 
  *reinterpret_cast<uint32_t*>(keyC + 9) = c_id; 
  return keyC;
}

std::string NewOrderRowKey(uint32_t w_id, uint32_t d_id, uint32_t o_id) {
  char keyC[13];
  keyC[0] = static_cast<char>(Tables::NEW_ORDER);
  *reinterpret_cast<uint32_t*>(keyC + 1) = w_id; 
  *reinterpret_cast<uint32_t*>(keyC + 5) = d_id; 
  *reinterpret_cast<uint32_t*>(keyC + 9) = o_id; 
  return keyC;
}

std::string OrderRowKey(uint32_t w_id, uint32_t d_id, uint32_t o_id) {
  char keyC[13];
  keyC[0] = static_cast<char>(Tables::ORDER);
  *reinterpret_cast<uint32_t*>(keyC + 1) = w_id; 
  *reinterpret_cast<uint32_t*>(keyC + 5) = d_id; 
  *reinterpret_cast<uint32_t*>(keyC + 9) = o_id; 
  return keyC;
}

std::string OrderLineRowKey(uint32_t w_id, uint32_t d_id, uint32_t o_id, uint32_t ol_number) {
  char keyC[17];
  keyC[0] = static_cast<char>(Tables::ORDER_LINE);
  *reinterpret_cast<uint32_t*>(keyC + 1) = w_id; 
  *reinterpret_cast<uint32_t*>(keyC + 5) = d_id; 
  *reinterpret_cast<uint32_t*>(keyC + 9) = o_id; 
  *reinterpret_cast<uint32_t*>(keyC + 13) = ol_number; 
  return keyC;
}

std::string ItemRowKey(uint32_t i_id) {
  char keyC[5];
  keyC[0] = static_cast<char>(Tables::ITEM);
  *reinterpret_cast<uint32_t*>(keyC + 1) = i_id; 
  return keyC;
}

std::string StockRowKey(uint32_t w_id, uint32_t i_id) {
  char keyC[9];
  keyC[0] = static_cast<char>(Tables::STOCK);
  *reinterpret_cast<uint32_t*>(keyC + 1) = w_id; 
  *reinterpret_cast<uint32_t*>(keyC + 5) = i_id; 
  return keyC;
}

std::string HistoryRowKey(uint32_t w_id, uint32_t d_id, uint32_t c_id) {
  char keyC[13];
  keyC[0] = static_cast<char>(Tables::HISTORY);
  *reinterpret_cast<uint32_t*>(keyC + 1) = w_id; 
  *reinterpret_cast<uint32_t*>(keyC + 5) = d_id; 
  *reinterpret_cast<uint32_t*>(keyC + 9) = c_id; 
  return keyC;
}

}
