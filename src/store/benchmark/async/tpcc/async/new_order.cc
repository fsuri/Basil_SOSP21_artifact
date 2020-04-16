#include "store/benchmark/async/tpcc/async/new_order.h"

#include <chrono>
#include <sstream>
#include <ctime>

#include "store/benchmark/async/tpcc/tpcc-proto.pb.h"
#include "store/benchmark/async/tpcc/tpcc_utils.h"

namespace tpcc {

AsyncNewOrder::AsyncNewOrder(uint32_t w_id, uint32_t C, uint32_t num_warehouses,
    std::mt19937 &gen) : NewOrder(w_id, C, num_warehouses, gen) {
  s_row = new StockRow[ol_cnt];
  i_row = new ItemRow[ol_cnt];
}

AsyncNewOrder::~AsyncNewOrder() {
  delete [] s_row;
  delete [] i_row;
}

Operation AsyncNewOrder::GetNextOperation(size_t opCount,
  std::map<std::string, std::string> readValues) {
  if (opCount == 0) {
    Debug("Warehouse: %u", w_id);
    return Get(WarehouseRowKey(w_id));
  } else if (opCount == 1) {
    Debug("District: %u", d_id);
    return Get(DistrictRowKey(w_id, d_id));
  } else if (opCount == 2) {
    std::string d_key = DistrictRowKey(w_id, d_id);
    auto d_row_itr = readValues.find(d_key);
    UW_ASSERT(d_row_itr != readValues.end());
    UW_ASSERT(d_row.ParseFromString(d_row_itr->second));

    o_id = d_row.next_o_id();
    Debug("Order Number: %u", o_id);
    d_row.set_next_o_id(d_row.next_o_id() + 1);

    std::string d_row_out;
    d_row.SerializeToString(&d_row_out);
    return Put(d_key, d_row_out);
  } else if (opCount == 3) {
    Debug("Customer: %u", c_id);
    return Get(CustomerRowKey(w_id, d_id, c_id));
  } else if (opCount == 4) {
    NewOrderRow no_row;
    no_row.set_o_id(o_id);
    no_row.set_d_id(d_id);
    no_row.set_w_id(w_id);

    std::string no_row_out;
    no_row.SerializeToString(&no_row_out);
    return Put(NewOrderRowKey(w_id, d_id, o_id), no_row_out);
  } else if (opCount == 5) {
    OrderRow o_row;
    o_row.set_id(o_id);
    o_row.set_d_id(d_id);
    o_row.set_w_id(w_id);
    o_row.set_c_id(c_id);
    o_row.set_entry_d(o_entry_d);
    o_row.set_carrier_id(0);
    o_row.set_ol_cnt(ol_cnt);
    o_row.set_all_local(all_local);

    std::string o_row_out;
    o_row.SerializeToString(&o_row_out);
    return Put(OrderRowKey(w_id, d_id, o_id), o_row_out);
  } else if (opCount == 6) {
    OrderByCustomerRow obc_row;
    obc_row.set_w_id(w_id);
    obc_row.set_d_id(d_id);
    obc_row.set_c_id(c_id);
    obc_row.set_o_id(o_id);

    std::string obc_row_out;
    obc_row.SerializeToString(&obc_row_out);
    return Put(OrderByCustomerRowKey(w_id, d_id, c_id), obc_row_out);
  } else if (static_cast<int32_t>(opCount) < 7 + 4 * ol_cnt) {
    int i = (opCount - 7) % 4;
    size_t ol_number = (opCount - 7) / 4;
    UW_ASSERT(o_ol_i_ids.size() > ol_number);
    UW_ASSERT(o_ol_supply_w_ids.size() > ol_number);
    UW_ASSERT(o_ol_quantities.size() > ol_number);
    if (i == 0) {
      Debug("  Order Line %lu", ol_number);
      Debug("    Item: %u", o_ol_i_ids[ol_number]);
      return Get(ItemRowKey(o_ol_i_ids[ol_number]));
    } else if (i == 1) {
      std::string i_key = ItemRowKey(o_ol_i_ids[ol_number]);
      auto i_row_itr = readValues.find(i_key);
      UW_ASSERT(i_row_itr != readValues.end());

      if(i_row_itr->second.empty()) {
        // i_id was invalid and returned empty string
        Debug("ABORT");
        return Abort();
      } else {
        Debug("    Item Name: %s", i_row[ol_number].name().c_str());
        Debug("    Supply Warehouse: %u", o_ol_supply_w_ids[ol_number]);
        return Get(StockRowKey(o_ol_supply_w_ids[ol_number],
            o_ol_i_ids[ol_number]));
      }
    } else if (i == 2) {
      std::string s_key = StockRowKey(o_ol_supply_w_ids[ol_number],
          o_ol_i_ids[ol_number]);
      auto s_row_itr = readValues.find(s_key);
      UW_ASSERT(s_row_itr != readValues.end());
      UW_ASSERT(s_row[ol_number].ParseFromString(s_row_itr->second));

      if (s_row[ol_number].quantity() - o_ol_quantities[ol_number] >= 10) {
        s_row[ol_number].set_quantity(s_row[ol_number].quantity() - o_ol_quantities[ol_number]);
      } else {
        s_row[ol_number].set_quantity(s_row[ol_number].quantity() - o_ol_quantities[ol_number] + 91);
      }
      Debug("    Quantity: %u", o_ol_quantities[ol_number]);
      s_row[ol_number].set_ytd(s_row[ol_number].ytd() + o_ol_quantities[ol_number]);
      s_row[ol_number].set_order_cnt(s_row[ol_number].order_cnt() + 1);
      Debug("    Remaining Quantity: %u", s_row[ol_number].quantity());
      Debug("    YTD: %u", s_row[ol_number].ytd());
      Debug("    Order Count: %u", s_row[ol_number].order_cnt());
      if (w_id != o_ol_supply_w_ids[ol_number]) {
        s_row[ol_number].set_remote_cnt(s_row[ol_number].remote_cnt() + 1);
      }

      std::string s_row_out;
      s_row[ol_number].SerializeToString(&s_row_out);
      return Put(s_key, s_row_out);
    } else {
      OrderLineRow ol_row;
      ol_row.set_o_id(o_id);
      ol_row.set_d_id(d_id);
      ol_row.set_w_id(w_id);
      ol_row.set_number(ol_number);
      ol_row.set_i_id(o_ol_i_ids[ol_number]);
      ol_row.set_supply_w_id(o_ol_supply_w_ids[ol_number]);
      ol_row.set_delivery_d(0);
      ol_row.set_quantity(o_ol_quantities[ol_number]);
      ol_row.set_amount(o_ol_quantities[ol_number] * i_row[ol_number].price());
      switch (d_id) {
        case 1:
          ol_row.set_dist_info(s_row[ol_number].dist_01());
          break;
        case 2:
          ol_row.set_dist_info(s_row[ol_number].dist_02());
          break;
        case 3:
          ol_row.set_dist_info(s_row[ol_number].dist_03());
          break;
        case 4:
          ol_row.set_dist_info(s_row[ol_number].dist_04());
          break;
        case 5:
          ol_row.set_dist_info(s_row[ol_number].dist_05());
          break;
        case 6:
          ol_row.set_dist_info(s_row[ol_number].dist_06());
          break;
        case 7:
          ol_row.set_dist_info(s_row[ol_number].dist_07());
          break;
        case 8:
          ol_row.set_dist_info(s_row[ol_number].dist_08());
          break;
        case 9:
          ol_row.set_dist_info(s_row[ol_number].dist_09());
          break;
        case 10:
          ol_row.set_dist_info(s_row[ol_number].dist_10());
          break;
      }

      std::string ol_row_out;
      ol_row.SerializeToString(&ol_row_out);
      return Put(OrderLineRowKey(w_id, d_id, o_id, ol_number), ol_row_out);
    }
  } else {
    Debug("COMMIT");
    return Commit();
  }
}

}
