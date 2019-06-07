#include "store/benchmark/async/tpcc/new_order.h"

#include <chrono>
#include <sstream>
#include <ctime>

#include "store/benchmark/async/tpcc/limits.h"
#include "store/benchmark/async/tpcc/tables.h"
#include "store/benchmark/async/tpcc/tpcc-proto.pb.h"
#include "store/benchmark/async/tpcc/tpcc_utils.h"

namespace tpcc {

NewOrder::NewOrder(uint32_t w_id, uint32_t C, uint32_t num_warehouses, std::mt19937 &gen) : w_id(w_id) {
  d_id = std::uniform_int_distribution<uint32_t>(1, 10)(gen); 
  c_id = NURand(static_cast<uint32_t>(1023), static_cast<uint32_t>(1), static_cast<uint32_t>(3000), C, gen);
  ol_cnt = std::uniform_int_distribution<uint8_t>(5, 15)(gen);
  rbk = std::uniform_int_distribution<uint8_t>(1, 100)(gen);
  all_local = true;
  for (uint8_t i = 0; i < ol_cnt; ++i) {
    if (rbk == 1 && i == ol_cnt - 1) {
      o_ol_i_ids.push_back(0);
    } else {
      o_ol_i_ids.push_back(NURand(static_cast<uint32_t>(8191), static_cast<uint32_t>(1), static_cast<uint32_t>(100000), C, gen));
    }
    uint8_t x = std::uniform_int_distribution<uint8_t>(1, 100)(gen);
    if (x == 1 && num_warehouses > 1) {
      uint32_t remote_w_id = std::uniform_int_distribution<uint32_t>(1, num_warehouses - 1)(gen);
      if (remote_w_id == w_id) {
        remote_w_id = num_warehouses; // simple swap to ensure uniform distribution
      }
      o_ol_supply_w_ids.push_back(remote_w_id);
      all_local = false;
    } else {
      o_ol_supply_w_ids.push_back(w_id);
    }
    o_ol_quantities.push_back(std::uniform_int_distribution<uint8_t>(1, 10)(gen));
  }
  o_entry_d = std::time(0);
  s_row = new StockRow[ol_cnt];
  i_row = new ItemRow[ol_cnt];
}

NewOrder::~NewOrder() {
  delete [] s_row;
  delete [] i_row;
}

Operation NewOrder::GetNextOperation(size_t opCount,
  std::map<std::string, std::string> readValues) {
  if (opCount == 0) {
    return Get(WarehouseRowKey(w_id));
  } else if (opCount == 1) {
    return Get(DistrictRowKey(w_id, d_id));
  } else if (opCount == 2) {
    std::string d_key = DistrictRowKey(w_id, d_id);
    auto d_row_itr = readValues.find(d_key);
    ASSERT(d_row_itr != readValues.end());
    ASSERT(d_row.ParseFromString(d_row_itr->second));

    o_id = d_row.next_o_id();
    d_row.set_next_o_id(d_row.next_o_id() + 1);

    std::string d_row_out;
    d_row.SerializeToString(&d_row_out);
    return Put(d_key, d_row_out);
  } else if (opCount == 3) {
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
  } else if (opCount < 6 + 4 * ol_cnt) {
    int i = (opCount - 6) % 4;
    size_t ol_number = (opCount - 6) / 4;
    Debug("OL NUMBER %d %d %d", ol_number, opCount, ol_cnt);
    ASSERT(o_ol_i_ids.size() > ol_number);
    ASSERT(o_ol_supply_w_ids.size() > ol_number);
    ASSERT(o_ol_quantities.size() > ol_number);
    if (i == 0) {
      return Get(ItemRowKey(o_ol_i_ids[ol_number]));
    } else if (i == 1) {
      std::string i_key = ItemRowKey(o_ol_i_ids[ol_number]);
      auto i_row_itr = readValues.find(i_key);
      ASSERT(i_row_itr != readValues.end());

      if(i_row[ol_number].ParseFromString(i_row_itr->second)) {
        Debug("Getting StockRow %d %d", o_ol_supply_w_ids[ol_number], o_ol_i_ids[ol_number]);
        return Get(StockRowKey(o_ol_supply_w_ids[ol_number], o_ol_i_ids[ol_number]));
      } else {
        // i_id was invalid and returned empty string
        return Abort();
      }
    } else if (i == 2) {
      std::string s_key = StockRowKey(o_ol_supply_w_ids[ol_number], o_ol_i_ids[ol_number]);
      auto s_row_itr = readValues.find(s_key);
      ASSERT(s_row_itr != readValues.end());
      Debug("Parsing StockRow %d %d", o_ol_supply_w_ids[ol_number], o_ol_i_ids[ol_number]);
      ASSERT(s_row[ol_number].ParseFromString(s_row_itr->second));

      if (s_row[ol_number].quantity() - o_ol_quantities[ol_number] >= 10) {
        s_row[ol_number].set_quantity(s_row[ol_number].quantity() - o_ol_quantities[ol_number]);
      } else {
        s_row[ol_number].set_quantity(s_row[ol_number].quantity() - o_ol_quantities[ol_number] + 91);
      }
      s_row[ol_number].set_ytd(s_row[ol_number].ytd() + o_ol_quantities[ol_number]);
      s_row[ol_number].set_order_cnt(s_row[ol_number].order_cnt() + 1);
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
    return Commit();
  }
}

}
