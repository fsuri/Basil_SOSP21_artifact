#include "store/benchmark/async/tpcc/payment.h"

#include <chrono>
#include <sstream>
#include <ctime>
#include <algorithm>

#include "store/benchmark/async/tpcc/tpcc-proto.pb.h"
#include "store/benchmark/async/tpcc/tpcc_utils.h"

namespace tpcc {

Payment::Payment(uint32_t w_id, uint32_t c_c_last, uint32_t c_c_id, uint32_t num_warehouses, std::mt19937 &gen) : w_id(w_id) {
  d_id = std::uniform_int_distribution<uint32_t>(1, 10)(gen); 
  d_w_id = w_id;
  int x = std::uniform_int_distribution<int>(1, 100)(gen);
  int y = std::uniform_int_distribution<int>(1, 100)(gen);
  if (x <= 85 || num_warehouses == 1) {
    c_w_id = w_id;
    c_d_id = d_id;
  } else {
    c_w_id = std::uniform_int_distribution<uint32_t>(1, num_warehouses - 1)(gen);
    if (c_w_id == w_id) {
      c_w_id = num_warehouses; // simple swap to ensure uniform distribution
    }
    c_d_id = std::uniform_int_distribution<uint32_t>(1, 10)(gen);
  }
  if (y <= 60) {
    int last = NURand(255, 0, 999, static_cast<int>(c_c_last), gen);
    c_last = GenerateCustomerLastName(last);
    c_by_last_name = true;
  } else {
    c_id = NURand(1023, 1, 3000, static_cast<int>(c_c_id), gen);
    c_by_last_name = false;
  }
  h_amount = std::uniform_int_distribution<uint32_t>(100, 500000)(gen);
  h_date = std::time(0);
}

Payment::~Payment() {
}

Operation Payment::GetNextOperation(size_t opCount,
  std::map<std::string, std::string> readValues) {
  if (opCount == 0) {
    Debug("Amount: %u", h_amount);
    Debug("Warehouse: %u", w_id);
    return Get(WarehouseRowKey(w_id));
  } else if (opCount == 1) {
    std::string w_key = WarehouseRowKey(w_id);
    auto w_row_itr = readValues.find(w_key);
    UW_ASSERT(w_row_itr != readValues.end());
    UW_ASSERT(w_row.ParseFromString(w_row_itr->second));

    w_row.set_ytd(w_row.ytd() + h_amount);
    Debug("  YTD: %u", w_row.ytd());

    std::string w_row_out;
    w_row.SerializeToString(&w_row_out);
    return Put(w_key, w_row_out);
  } else if (opCount == 2) {
    Debug("District: %u", d_id);
    return Get(DistrictRowKey(d_w_id, d_id));
  } else if (opCount == 3) {
    std::string d_key = DistrictRowKey(d_w_id, d_id);
    auto d_row_itr = readValues.find(d_key);
    UW_ASSERT(d_row_itr != readValues.end());
    UW_ASSERT(d_row.ParseFromString(d_row_itr->second));

    d_row.set_ytd(d_row.ytd() + h_amount);
    Debug("  YTD: %u", d_row.ytd());

    std::string d_row_out;
    d_row.SerializeToString(&d_row_out);
    return Put(d_key, d_row_out);
  } else if (opCount == 4) {
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
      if (opCount == 5) {
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

    if (count == 5) {
      std::string c_key = CustomerRowKey(c_w_id, c_d_id, c_id);
      auto c_row_itr = readValues.find(c_key);
      UW_ASSERT(c_row_itr != readValues.end());
      UW_ASSERT(c_row.ParseFromString(c_row_itr->second));

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

      std::string c_row_out;
      c_row.SerializeToString(&c_row_out);
      return Put(c_key, c_row_out);
    } else if (count == 6) {
      HistoryRow h_row;
      h_row.set_c_id(c_id);
      h_row.set_c_d_id(c_d_id);
      h_row.set_c_w_id(c_w_id);
      h_row.set_d_id(d_id);
      h_row.set_w_id(w_id);
      h_row.set_data(w_row.name() + "    " + d_row.name());

      std::string h_row_out;
      h_row.SerializeToString(&h_row_out);
      return Put(HistoryRowKey(w_id, d_id, c_id), h_row_out);
    } else {
      Debug("COMMIT");
      return Commit();
    }
  }
  }
}

