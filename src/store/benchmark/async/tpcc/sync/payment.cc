#include "store/benchmark/async/tpcc/sync/payment.h"

#include <sstream>

#include "store/benchmark/async/tpcc/tpcc_utils.h"

namespace tpcc {

SyncPayment::SyncPayment(uint32_t w_id, uint32_t c_c_last, uint32_t c_c_id,
      uint32_t num_warehouses, std::mt19937 &gen) : Payment(w_id, c_c_last,
        c_c_id, num_warehouses, gen) {
}

SyncPayment::~SyncPayment() {
}

int SyncPayment::Execute(SyncClient &client) {
  std::string str;

  Debug("Amount: %u", h_amount);
  Debug("Warehouse: %u", w_id);
  
  std::string w_key = WarehouseRowKey(w_id);
  client.Get(w_key, str, timeout);
  WarehouseRow w_row;
  UW_ASSERT(w_row.ParseFromString(str));
  w_row.set_ytd(w_row.ytd() + h_amount);
  Debug("  YTD: %u", w_row.ytd());
  w_row.SerializeToString(&str);
  client.Put(w_key, str, timeout);
  
  Debug("District: %u", d_id);
  std::string d_key = DistrictRowKey(d_w_id, d_id);
  client.Get(d_key, str, timeout);
  DistrictRow d_row;
  UW_ASSERT(d_row.ParseFromString(str));
  d_row.set_ytd(d_row.ytd() + h_amount);
  Debug("  YTD: %u", d_row.ytd());
  d_row.SerializeToString(&str);
  client.Put(d_key, str, timeout);

  if (c_by_last_name) { // access customer by last name
    Debug("Customer: %s", c_last.c_str());
    Debug("  Get(c_w_id=%u, c_d_id=%u, c_last=%s)", c_w_id, c_d_id,
      c_last.c_str());
    std::string cbn_key = CustomerByNameRowKey(c_w_id, c_d_id, c_last);
    client.Get(cbn_key, str, timeout);
    CustomerByNameRow cbn_row;
    UW_ASSERT(cbn_row.ParseFromString(str));
    int idx = (cbn_row.ids_size() + 1) / 2;
    if (idx == cbn_row.ids_size()) {
      idx = cbn_row.ids_size() - 1;
    }
    c_id = cbn_row.ids(idx);
    Debug("  ID: %u", c_id);
  } else {
    Debug("Customer: %u", c_id);
  }
  std::string c_key = CustomerRowKey(c_w_id, c_d_id, c_id);
  client.Get(CustomerRowKey(c_w_id, c_d_id, c_id), str, timeout);

  CustomerRow c_row;
  UW_ASSERT(c_row.ParseFromString(str));
  c_row.set_balance(c_row.balance() - h_amount);
  c_row.set_ytd_payment(c_row.ytd_payment() + h_amount);
  c_row.set_payment_cnt(c_row.payment_cnt() + 1);
  Debug("  Balance: %u", c_row.balance());
  Debug("  YTD: %u", c_row.ytd_payment());
  Debug("  Payment Count: %u", c_row.payment_cnt());
  if (c_row.credit() == "BC") {
    std::stringstream ss;
    ss << c_id << "," << c_d_id << "," << c_w_id << "," << d_id << ","
             << w_id << "," << h_amount; 
    std::string new_data = ss.str() +  c_row.data();
    new_data = new_data.substr(std::min(new_data.size(), 500UL));
    c_row.set_data(new_data);
  }
  c_row.SerializeToString(&str);
  client.Put(c_key, str, timeout);

  HistoryRow h_row;
  h_row.set_c_id(c_id);
  h_row.set_c_d_id(c_d_id);
  h_row.set_c_w_id(c_w_id);
  h_row.set_d_id(d_id);
  h_row.set_w_id(w_id);
  h_row.set_data(w_row.name() + "    " + d_row.name());
  h_row.SerializeToString(&str);
  client.Put(HistoryRowKey(w_id, d_id, c_id), str, timeout);

  Debug("COMMIT");
  return client.Commit(timeout);
}

} // namespace tpcc
