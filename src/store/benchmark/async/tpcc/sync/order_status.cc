#include "store/benchmark/async/tpcc/sync/order_status.h"

#include "store/benchmark/async/tpcc/tpcc_utils.h"

namespace tpcc {

SyncOrderStatus::SyncOrderStatus(uint32_t w_id, uint32_t c_c_last,
    uint32_t c_c_id, std::mt19937 &gen) : OrderStatus(w_id, c_c_last,
      c_c_id, gen) {
}

SyncOrderStatus::~SyncOrderStatus() {
}

int SyncOrderStatus::Execute(SyncClient &client) {
  std::string str;

  Debug("ORDER_STATUS");
  Debug("Warehouse: %u", c_w_id);
  Debug("District: %u", c_d_id);
  if (c_by_last_name) { // access customer by last name
    Debug("Customer: %s", c_last.c_str());
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
  client.Get(c_key, str, timeout);
  CustomerRow c_row;
  UW_ASSERT(c_row.ParseFromString(str));
  Debug("  First: %s", c_row.first().c_str());
  Debug("  Last: %s", c_row.last().c_str());

  std::string obc_key = OrderByCustomerRowKey(c_w_id, c_d_id, c_id);
  client.Get(obc_key, str, timeout);
  OrderByCustomerRow obc_row;
  UW_ASSERT(obc_row.ParseFromString(str));

  o_id = obc_row.o_id();
  Debug("Order: %u", o_id);
  std::string o_key = OrderRowKey(c_w_id, c_d_id, o_id);
  client.Get(o_key, str, timeout);
  OrderRow o_row;
  UW_ASSERT(o_row.ParseFromString(str));
  Debug("  Order Lines: %u", o_row.ol_cnt());
  Debug("  Entry Date: %u", o_row.entry_d());
  Debug("  Carrier ID: %u", o_row.carrier_id());

  for (size_t ol_number = 0; ol_number < o_row.ol_cnt(); ++ol_number) {
    client.Get(OrderLineRowKey(c_w_id, c_d_id, o_id, ol_number), str, timeout);
  }

  Debug("COMMIT");
  return client.Commit(timeout);
}

} // namespace tpcc
