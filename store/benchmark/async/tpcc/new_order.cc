#include "store/benchmark/async/tpcc/new_order.h"

#include <chrono>
#include <sstream>

#include "store/benchmark/async/tpcc/limits.h"
#include "store/benchmark/async/tpcc/tables.h"

namespace tpcc {
namespace {

using std::string;
using std::to_string;
using std::vector;

// Join the string with delimiter
string Join(const vector<string>& strs) {
  constexpr char delimiter = '-';
  string res;
  for (size_t i = 0; i < strs.size() - 1; ++i) {
    res += strs[i] + delimiter;
  }
  res += strs.back();
  return res;
}

// Split the string with delimiter
vector<string> Split(const string& str) {
  constexpr char delimiter = '-';
  vector<string> res;
  string token;
  std::istringstream stream(str);
  while (std::getline(stream, token, delimiter)) {
    res.push_back(token);
  }
  return res;
}

string TimeStamp() {
  using namespace std::chrono;
  milliseconds ms = duration_cast<milliseconds>(
      system_clock::now().time_since_epoch());
  return to_string(ms.count());
}

} /* namespace */

NewOrder::NewOrder()
    : TPCCTransaction(), generator_(new RealRandomGenerator()) {
  int w_id = generator_->number(1, limits::Warehouse::MAX_WAREHOUSE_ID);
  int d_id = generator_->number(1, limits::District::NUM_PER_WAREHOUSE);
  int c_id = generator_->NURand(1023, 1, limits::Customer::NUM_PER_DISTRICT);
  int ol_cnt = generator_->number(limits::Order::MIN_OL_CNT,
                                  limits::Order::MAX_OL_CNT);

  // 1% of transactions roll back
  bool rollback = generator_->number(1, 100) == 1;
  bool all_local = true;
  vector<string> i_ids;
  vector<string> i_w_ids;
  vector<string> i_qtys;
  for (int i = 0; i < ol_cnt; ++i) {
    int i_id;
    if (rollback && (i + 1) == ol_cnt) {
      i_id = limits::Item::NUM_ITEMS + 1;
    } else {
      i_id = generator_->NURand(8191, 1, limits::Item::NUM_ITEMS);
    }
    i_ids.push_back(to_string(i_id));

    // TPC-C suggests generating a number in range (1, 100) and selecting remote on 1
    // This provides more variation, and lets us tune the fraction of "remote" transactions.
    int i_w_id;
    if (generator_->number(1, 1000) <= limits::OrderLine::REMOTE_PROBABILITY_MILLIS) {
      i_w_id = generator_->numberExcluding(1, limits::Warehouse::MAX_WAREHOUSE_ID, w_id);
      all_local = false;
    } else {
      i_w_id = w_id;
    }
    i_w_ids.push_back(to_string(i_w_id));

    int i_qty = generator_->number(1, limits::OrderLine::MAX_OL_QUANTITY);
    i_qtys.push_back(to_string(i_qty));
  }

  lists_["i_ids"] = i_ids;
  lists_["i_w_ids"] = i_w_ids;
  lists_["i_qtys"] = i_qtys;
  params_["w_id"] = to_string(w_id);
  params_["d_id"] = to_string(d_id);
  params_["c_id"] = to_string(c_id);
  params_["o_entry_d"] = TimeStamp();
  params_["o_carrier_id"] = "0";
  params_["ol_cnt"] = to_string(ol_cnt);
  params_["all_local"] = all_local ? "1" : "0";
  params_["total"] = "0";
}

NewOrder::~NewOrder() {
}

Operation NewOrder::GetNextOperation(size_t opCount,
    const std::map<std::string, std::string> &readValues) {
  if (opCount == 0) {
    // Get Warehouse Tax Rate
    last_query_ = Join({"WAREHOUSE",
                        params_["w_id"]});  // W_ID
    return Get(last_query_);
  } else if (opCount == 1) {
    auto res = Split(readValues.find(last_query_)->second);
    params_["w_tax"] = res[W_TAX];

    // Get District Tax Rate and Next Order ID
    last_query_ = Join({"DISTRICT",
                        params_["w_id"],    // D_W_ID
                        params_["d_id"]});  // D_ID
    return Get(last_query_);
  } else if (opCount == 2) {
    auto res = Split(readValues.find(last_query_)->second);
    params_["d_tax"] = res[D_TAX];
    params_["d_next_o_id"] = res[D_NEXT_O_ID];

    // Increment District Next Order ID
    res[D_NEXT_O_ID] = to_string(std::stoi(params_["d_next_o_id"]) + 1);
    return Put(last_query_, Join(res));

  } else if (opCount == 3) {
    // Get Customer Discount
    last_query_ = Join({"CUSTOMER",
                        params_["w_id"],    // C_W_ID
                        params_["d_id"],    // C_D_ID
                        params_["c_id"]});  // C_ID
    return Get(last_query_);
  } else if (opCount == 4) {
    auto res = Split(readValues.find(last_query_)->second);
    params_["c_discount"] = res[C_DISCOUNT];

    // Create Order
    params_["order"] = Join({"ORDER",
                             params_["w_id"],           // O_W_ID
                             params_["d_id"],           // O_D_ID
                             params_["d_next_o_id"]});  // O_ID
    return Put(params_["order"],
               Join({params_["d_next_o_id"],    // O_ID
                     params_["d_id"],           // O_D_ID
                     params_["w_id"],           // O_W_ID
                     params_["c_id"],           // O_C_ID
                     params_["o_entry_d"],      // O_ENTRY_D
                     params_["o_carrier_id"],   // O_CARRIER_ID
                     params_["ol_cnt"],         // O_OL_CNT
                     params_["all_local"]}));   // O_ALL_LOCAL

  } else if (opCount == 5) {
    // Create New Order
    params_["new_order"] = Join({"NEW_ORDER",
                                 params_["w_id"],           // NO_W_ID
                                 params_["d_id"],           // NO_D_ID
                                 params_["d_next_o_id"]});  // NO_O_ID
    return Put(params_["new_order"],
               Join({params_["d_next_o_id"],  // NO_O_ID
                     params_["d_id"],         // NO_D_ID
                     params_["w_id"]}));       // NO_W_ID
  } else {
    int ol_cnt = std::stoi(params_["ol_cnt"]);
    int index = (opCount - 6) / 4;
    int step = (opCount - 6) % 4;
    if (step == 0 && index == ol_cnt) {
      return Commit();
    } else if (step == 0) {
      // Get Stock Info
      last_query_ = Join({"STOCK",
                          lists_["i_w_ids"][index],   // S_W_ID
                          lists_["i_ids"][index]});   // S_I_ID
      return Get(last_query_);
    } else if (step == 1) {
      auto res = Split(readValues.find(last_query_)->second);

      // Update Stock
      int ol_quantity = std::stoi(lists_["i_qtys"][index]);
      int s_quantity = std::stoi(res[S_QUANTITY]);
      int s_ytd = std::stoi(res[S_YTD]);
      int s_order_cnt = std::stoi(res[S_ORDER_CNT]);
      int s_remote_cnt = std::stoi(res[S_REMOTE_CNT]);

      s_ytd += ol_quantity;
      if (s_quantity > ol_quantity + 10) {
        s_quantity = s_quantity - ol_quantity;
      } else {
        s_quantity = s_quantity + 91 - ol_quantity;
      }
      ++s_order_cnt;
      if (lists_["i_w_ids"][index] != params_["w_id"]) {
        ++s_remote_cnt;
      }
      params_["s_dist_xx"] = res[S_DIST_01 + std::stoi(params_["d_id"]) - 1];

      res[S_YTD] = to_string(s_ytd);
      res[S_QUANTITY] = to_string(s_quantity);
      res[S_ORDER_CNT] = to_string(s_order_cnt);
      res[S_REMOTE_CNT] = to_string(s_remote_cnt);
      return Put(last_query_, Join(res));
    } else if (step == 2) {
      // Query Item
      last_query_ = Join({"ITEM",
                          lists_["i_ids"][index]});   // I_ID
      return Get(last_query_);
    } else if (step == 3) {
      auto res = Split(readValues.find(last_query_)->second);

      // Create New Order Line
      int ol_quantity = std::stoi(lists_["i_qtys"][index]);
      float i_price = std::stof(res[I_PRICE]);
      int ol_amount = ol_quantity * i_price;
      params_["total"] = to_string(std::stoi(params_["total"]) + ol_amount);
      string ol_number = to_string(index + 1);
      last_query_ = Join({"ORDER_LINE",
                          params_["w_id"],          // OL_W_ID
                          params_["d_id"],          // OL_D_ID
                          params_["d_next_o_id"],   // OL_O_ID
                          ol_number});              // OL_NUMBER add a "-"!!!!!
      return Put(last_query_,Join({params_["d_next_o_id"],     // OL_O_ID
                  params_["d_id"],            // OL_D_ID
                  params_["w_id"],            // OL_W_ID
                  ol_number,                  // OL_NUMBER
                  lists_["i_id"][index],      // OL_I_ID
                  lists_["i_w_ids"][index],   // OL_SUPPLY_W_ID
                  params_["o_entry_d"],       // OL_DELIVERY_D
                  lists_["i_qtys"][index],    // OL_QUANTITY
                  to_string(ol_amount),  // OL_AMOUNT
                  params_["s_dist_xx"]}));    // OL_DIST_INFO
    }
  }

  // Should Not Come Here
  return Abort();
}

}
