#include "store/benchmark/async/tpcc/async/order_status.h"

#include <chrono>
#include <sstream>
#include <ctime>

#include "store/benchmark/async/tpcc/tpcc-proto.pb.h"
#include "store/benchmark/async/tpcc/tpcc_utils.h"

namespace tpcc {

AsyncOrderStatus::AsyncOrderStatus(uint32_t w_id, uint32_t c_c_last,
    uint32_t c_c_id, std::mt19937 &gen) : OrderStatus(w_id, c_c_last, c_c_id,
      gen) {
}

AsyncOrderStatus::~AsyncOrderStatus() {
}

Operation AsyncOrderStatus::GetNextOperation(size_t opCount,
  std::map<std::string, std::string> readValues) {
  if (opCount == 0) {
    Debug("ORDER_STATUS");
    Debug("Warehouse: %u", c_w_id);
    Debug("District: %u", c_d_id);
    if (c_by_last_name) { // access customer by last name
      Debug("Customer: %s", c_last.c_str());
      return Get(CustomerByNameRowKey(c_w_id, c_d_id, c_last));
    } else {
      Debug("Customer: %u", c_id);
      return Get(CustomerRowKey(c_w_id, c_d_id, c_id));
    }
  } else {
    uint32_t count;
    if (c_by_last_name) {
      if (opCount == 1) {
        std::string cbn_key = CustomerByNameRowKey(c_w_id, c_d_id, c_last);
        auto cbn_row_itr = readValues.find(cbn_key);
        UW_ASSERT(cbn_row_itr != readValues.end());
        UW_ASSERT(cbn_row.ParseFromString(cbn_row_itr->second));

        int idx = (cbn_row.ids_size() + 1) / 2;
        if (idx == cbn_row.ids_size()) {
          idx = cbn_row.ids_size() - 1;
        }
        c_id = cbn_row.ids(idx);
        Debug("  ID: %u", c_id);

        return Get(CustomerRowKey(c_w_id, c_d_id, c_id));
      }
      count = opCount - 1;
    } else {
      count = opCount;
    }

    if (count == 1) {
      std::string c_key = CustomerRowKey(c_w_id, c_d_id, c_id);
      auto c_row_itr = readValues.find(c_key);
      UW_ASSERT(c_row_itr != readValues.end());
      UW_ASSERT(c_row.ParseFromString(c_row_itr->second));

      Debug("  First: %s", c_row.first().c_str());
      Debug("  Last: %s", c_row.last().c_str());

      return Get(OrderByCustomerRowKey(c_w_id, c_d_id, c_id));
    } else if (count == 2) {
      std::string obc_key = OrderByCustomerRowKey(c_w_id, c_d_id, c_id);
      auto obc_row_itr = readValues.find(obc_key);
      UW_ASSERT(obc_row_itr != readValues.end());
      UW_ASSERT(obc_row.ParseFromString(obc_row_itr->second));

      o_id = obc_row.o_id();
      Debug("Order: %u", o_id);

      return Get(OrderRowKey(c_w_id, c_d_id, o_id));
    } else {
      if (count == 3) {
        std::string o_key = OrderRowKey(c_w_id, c_d_id, o_id);
        auto o_row_itr = readValues.find(o_key);
        UW_ASSERT(o_row_itr != readValues.end());
        UW_ASSERT(o_row.ParseFromString(o_row_itr->second));

        Debug("  Order Lines: %u", o_row.ol_cnt());
        Debug("  Entry Date: %u", o_row.entry_d());
        Debug("  Carrier ID: %u", o_row.carrier_id());
      }

      if (count < 2 + o_row.ol_cnt()) {
        uint32_t ol_number = count - 2;
        return Get(OrderLineRowKey(c_w_id, c_d_id, o_id, ol_number));
      } else {
        Debug("COMMIT");
        return Commit();
      }
    }
  }
}

}
