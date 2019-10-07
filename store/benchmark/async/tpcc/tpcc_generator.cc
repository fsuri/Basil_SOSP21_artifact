#include <condition_variable>
#include <mutex>
#include <queue>
#include <random>
#include <string>
#include <utility>
#include <numeric>
#include <algorithm>

#include <gflags/gflags.h>

#include "lib/io_utils.h"
#include "store/benchmark/async/tpcc/tpcc_utils.h"
#include "store/benchmark/async/tpcc/tpcc-proto.pb.h"

template<class T>
class Queue {
 public:
  Queue(size_t maxSize) : maxSize(maxSize) { }

  void Push(const T &t) {
    std::unique_lock<std::mutex> lock(mtx);
    while (IsFull()) {
      cond.wait(lock);
    }
    q.push(t);
  }

  void Pop(T &t) {
    std::unique_lock<std::mutex> lock(mtx);
    while (IsEmpty()) {
      cond.wait(lock);
    }
    t = q.front();
    q.pop();
    cond.notify_one();
  }

  bool IsEmpty() {
    return q.size() == 0;
  }
 private:
  bool IsFull() {
    return q.size() == maxSize;
  }

  std::queue<T> q;
  std::mutex mtx;
  std::condition_variable cond;
  size_t maxSize;
};

const char ALPHA_NUMERIC[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

std::string RandomAString(size_t x, size_t y, std::mt19937 &gen) {
  std::string s;
  size_t length = std::uniform_int_distribution<size_t>(x,  y)(gen);
  for (size_t i = 0; i < length; ++i) {
    int j = std::uniform_int_distribution<size_t>(0, sizeof(ALPHA_NUMERIC))(gen);
    s += ALPHA_NUMERIC[j];
  }
  return s;
}

std::string RandomNString(size_t x, size_t y, std::mt19937 &gen) {
  std::string s;
  size_t length = std::uniform_int_distribution<size_t>(x,  y)(gen);
  for (size_t i = 0; i < length; ++i) {
    int j = std::uniform_int_distribution<size_t>(0, 9)(gen);
    s += ALPHA_NUMERIC[j];
  }
  return s;
}

std::string RandomZip(std::mt19937 &gen) {
  return RandomNString(4, 4, gen) + "11111";
}

const char ORIGINAL_CHARS[] = "ORIGINAL";

void GenerateItemTable(Queue<std::pair<std::string, std::string>> &q) {
  std::mt19937 gen;
  tpcc::ItemRow i_row;
  std::string i_row_out;
  for (uint32_t i_id = 1; i_id <= 100000; ++i_id) {
    i_row.set_id(i_id);
    i_row.set_im_id(std::uniform_int_distribution<uint32_t>(1, 10000)(gen));
    i_row.set_name(RandomAString(14, 24, gen));
    i_row.set_price(std::uniform_int_distribution<uint32_t>(100, 10000)(gen));
    std::string data = RandomAString(26, 50, gen);
    int original = std::uniform_int_distribution<uint32_t>(1, 10)(gen); 
    if (original == 1) {
      int startIdx = std::uniform_int_distribution<int>(0, data.length() - sizeof(ORIGINAL_CHARS))(gen);
      data.replace(startIdx, sizeof(ORIGINAL_CHARS), ORIGINAL_CHARS); 
    }
    i_row.set_data(data);
    i_row.SerializeToString(&i_row_out);
    std::string i_key = tpcc::ItemRowKey(i_id);
    q.Push(std::make_pair(i_key, i_row_out));
  }
}

void GenerateWarehouseTable(uint32_t num_warehouses,
    Queue<std::pair<std::string, std::string>> &q) {
  std::mt19937 gen;
  tpcc::WarehouseRow w_row;
  std::string w_row_out;
  for (uint32_t w_id = 1; w_id <= num_warehouses; ++w_id) {
    w_row.set_id(w_id);
    w_row.set_name(RandomAString(6, 10, gen));
    w_row.set_street_1(RandomAString(10, 20, gen));
    w_row.set_street_2(RandomAString(10, 20, gen));
    w_row.set_city(RandomAString(10, 20, gen));
    w_row.set_state(RandomAString(2, 2, gen));
    w_row.set_zip(RandomZip(gen));
    w_row.set_tax(std::uniform_int_distribution<uint32_t>(0, 2000)(gen));
    w_row.set_ytd(30000000);
    w_row.SerializeToString(&w_row_out);
    std::string w_key = tpcc::WarehouseRowKey(w_id);
    q.Push(std::make_pair(w_key, w_row_out));    
  }
}

void GenerateStockTableForWarehouse(uint32_t w_id,
    Queue<std::pair<std::string, std::string>> &q) {
  std::mt19937 gen;
  tpcc::StockRow s_row;
  std::string s_row_out;
  for (uint32_t s_i_id = 1; s_i_id <= 100000; ++s_i_id) {
    s_row.set_i_id(s_i_id);
    s_row.set_w_id(w_id);
    s_row.set_quantity(std::uniform_int_distribution<uint32_t>(10, 100)(gen));
    s_row.set_dist_01(RandomAString(24, 24, gen));
    s_row.set_dist_02(RandomAString(24, 24, gen));
    s_row.set_dist_03(RandomAString(24, 24, gen));
    s_row.set_dist_04(RandomAString(24, 24, gen));
    s_row.set_dist_05(RandomAString(24, 24, gen));
    s_row.set_dist_06(RandomAString(24, 24, gen));
    s_row.set_dist_07(RandomAString(24, 24, gen));
    s_row.set_dist_08(RandomAString(24, 24, gen));
    s_row.set_dist_09(RandomAString(24, 24, gen));
    s_row.set_dist_10(RandomAString(24, 24, gen));
    s_row.set_ytd(0);
    s_row.set_order_cnt(0);
    s_row.set_remote_cnt(0);
    std::string data = RandomAString(26, 50, gen);
    int original = std::uniform_int_distribution<uint32_t>(1, 10)(gen); 
    if (original == 1) {
      int startIdx = std::uniform_int_distribution<int>(0, data.length() - sizeof(ORIGINAL_CHARS))(gen);
      data.replace(startIdx, sizeof(ORIGINAL_CHARS), ORIGINAL_CHARS); 
    }
    s_row.set_data(data);
    s_row.SerializeToString(&s_row_out);
    std::string s_key = tpcc::StockRowKey(w_id, s_i_id);
    q.Push(std::make_pair(s_key, s_row_out));    
  }
}

void GenerateStockTable(uint32_t num_warehouses,
    Queue<std::pair<std::string, std::string>> &q) {
  for (uint32_t w_id = 1; w_id <= num_warehouses; ++w_id) {
    GenerateStockTableForWarehouse(w_id, q);
  }
}

void GenerateDistrictTableForWarehouse(uint32_t w_id,
    Queue<std::pair<std::string, std::string>> &q) {
  std::mt19937 gen;
  tpcc::DistrictRow d_row;
  std::string d_row_out;
  for (uint32_t d_id = 1; d_id <= 10; ++d_id) {
    d_row.set_id(d_id);
    d_row.set_w_id(w_id);
    d_row.set_name(RandomAString(6, 10, gen));
    d_row.set_street_1(RandomAString(10, 20, gen));
    d_row.set_street_2(RandomAString(10, 20, gen));
    d_row.set_city(RandomAString(10, 20, gen));
    d_row.set_state(RandomAString(2, 2, gen));
    d_row.set_zip(RandomZip(gen));
    d_row.set_tax(std::uniform_int_distribution<uint32_t>(0, 2000)(gen));
    d_row.set_ytd(3000000);
    d_row.set_next_o_id(3001);
    d_row.SerializeToString(&d_row_out);
    std::string d_key = tpcc::DistrictRowKey(w_id, d_id);
    q.Push(std::make_pair(d_key, d_row_out));    
  }
}

void GenerateDistrictTable(uint32_t num_warehouses,
    Queue<std::pair<std::string, std::string>> &q) {
  for (uint32_t w_id = 1; w_id <= num_warehouses; ++w_id) {
    GenerateDistrictTableForWarehouse(w_id, q);
  }
}

void GenerateCustomerTableForWarehouseDistrict(uint32_t w_id, uint32_t d_id,
    uint32_t time, uint32_t c_last, Queue<std::pair<std::string, std::string>> &q) {
  std::mt19937 gen;
  tpcc::CustomerRow c_row;
  std::string c_row_out;

  std::map<std::string, std::set<std::pair<std::string, uint32_t>>> custWithLast;
  for (uint32_t c_id = 1; c_id <= 3000; ++c_id) {
    c_row.set_id(c_id);
    c_row.set_d_id(d_id);
    c_row.set_w_id(w_id);
    int last;
    if (c_id <= 1000) {
      last = c_id - 1;
    } else {
      last = tpcc::NURand(255, 0, 999, static_cast<int>(c_last), gen); 
    }
    c_row.set_last(tpcc::GenerateCustomerLastName(last));
    c_row.set_middle("OE");
    c_row.set_first(RandomAString(8, 16, gen));
    custWithLast[c_row.last()].insert(std::make_pair(c_row.first(), c_id));
    c_row.set_street_1(RandomAString(10, 20, gen));
    c_row.set_street_2(RandomAString(10, 20, gen));
    c_row.set_city(RandomAString(10, 20, gen));
    c_row.set_state(RandomAString(2, 2, gen));
    c_row.set_zip(RandomZip(gen));
    c_row.set_phone(RandomNString(16, 16, gen));
    c_row.set_since(time);
    int credit = std::uniform_int_distribution<int>(1, 10)(gen);
    if (credit == 1) {
      c_row.set_credit("BC");
    } else {
      c_row.set_credit("GC");
    }
    c_row.set_credit_lim(5000000);
    c_row.set_discount(std::uniform_int_distribution<int>(0, 5000)(gen));
    c_row.set_balance(-1000);
    c_row.set_ytd_payment(1000);
    c_row.set_payment_cnt(1);
    c_row.set_delivery_cnt(0);
    c_row.set_data(RandomAString(300, 500, gen));
    c_row.SerializeToString(&c_row_out);
    std::string c_key = tpcc::CustomerRowKey(w_id, d_id, c_id);
    q.Push(std::make_pair(c_key, c_row_out));    
  }

  tpcc::CustomerByNameRow cbn_row;
  std::string cbn_row_out;
  for (auto cwl : custWithLast) {
    cbn_row.set_w_id(w_id);
    cbn_row.set_d_id(d_id);
    cbn_row.set_last(cwl.first);
    cbn_row.clear_ids();
    for (auto id : cwl.second) {
      cbn_row.add_ids(id.second);
    }
    cbn_row.SerializeToString(&cbn_row_out);
    std::string cbn_key = tpcc::CustomerByNameRowKey(w_id, d_id, cwl.first);
    q.Push(std::make_pair(cbn_key, cbn_row_out));
  }
}

void GenerateCustomerTable(uint32_t num_warehouses, uint32_t c_load_c_last,
    uint32_t time, Queue<std::pair<std::string, std::string>> &q) {
  std::mt19937 gen;
  for (uint32_t w_id = 1; w_id <= num_warehouses; ++w_id) {
    for (uint32_t d_id = 1; d_id <= 10; ++d_id) {
      GenerateCustomerTableForWarehouseDistrict(w_id, d_id, time, c_load_c_last,
          q);
    }
  }
}

void GenerateHistoryTable(uint32_t num_warehouses, 
    Queue<std::pair<std::string, std::string>> &q) {
  std::mt19937 gen;
  tpcc::HistoryRow h_row;
  std::string h_row_out;
  for (uint32_t w_id = 1; w_id <= num_warehouses; ++w_id) {
    for (uint32_t d_id = 1; d_id <= 10; ++d_id) {
      for (uint32_t c_id = 1; c_id <= 3000; ++c_id) {
        h_row.set_c_id(c_id);
        h_row.set_d_id(d_id);
        h_row.set_w_id(w_id);
        h_row.set_date(std::time(0));
        h_row.set_amount(1000);
        h_row.set_data(RandomAString(12, 24, gen));
        h_row.SerializeToString(&h_row_out);
        std::string h_key = tpcc::HistoryRowKey(w_id, d_id, c_id);
        q.Push(std::make_pair(h_key, h_row_out));    
      }
    }
  }
}

void GenerateOrderTableForWarehouseDistrict(uint32_t w_id, uint32_t d_id,
    uint32_t c_load_ol_i_id, Queue<std::pair<std::string, std::string>> &q) {
  std::mt19937 gen;
  tpcc::OrderRow o_row;
  std::string o_row_out;
  tpcc::OrderLineRow ol_row;
  std::string ol_row_out;
  tpcc::OrderByCustomerRow obc_row;
  std::string obc_row_out;
  std::vector<uint32_t> c_ids(3000);
  std::iota(c_ids.begin(), c_ids.end(), 1);
  std::shuffle(c_ids.begin(), c_ids.end(), gen);
  for (uint32_t i = 0; i < 3000; ++i) {
    uint32_t c_id = c_ids[i];
    uint32_t o_id = i + 1;
    o_row.set_id(o_id);
    o_row.set_c_id(c_id);
    o_row.set_d_id(d_id);
    o_row.set_w_id(w_id);
    o_row.set_entry_d(std::time(0));
    if (o_id < 2101) {
      o_row.set_carrier_id(std::uniform_int_distribution<uint32_t>(1, 10)(gen));
    } else {
      o_row.set_carrier_id(0);
    }
    o_row.set_ol_cnt(std::uniform_int_distribution<uint32_t>(5, 15)(gen));
    o_row.set_all_local(true);
    o_row.SerializeToString(&o_row_out);
    std::string o_key = tpcc::OrderRowKey(w_id, d_id, c_id);
    q.Push(std::make_pair(o_key, o_row_out));    
    
    // initially, there is exactly one order per customer, so we do not need to
    // worry about writing multiple OrderByCustomerRow with the same key.
    obc_row.set_w_id(w_id);
    obc_row.set_d_id(d_id);
    obc_row.set_c_id(c_id);
    obc_row.set_o_id(o_id);
    obc_row.SerializeToString(&obc_row_out);
    std::string obc_key = tpcc::OrderByCustomerRowKey(w_id, d_id, c_id);
    q.Push(std::make_pair(obc_key, obc_row_out));
    for (uint32_t ol_number = 0; ol_number < o_row.ol_cnt(); ++ol_number) {
      ol_row.set_o_id(o_id);
      ol_row.set_d_id(d_id);
      ol_row.set_w_id(w_id);
      ol_row.set_number(ol_number);
      ol_row.set_i_id(tpcc::NURand(8191, 1, 100000,
          static_cast<int>(c_load_ol_i_id), gen));
      ol_row.set_supply_w_id(w_id);
      if (o_id < 2101) {
        ol_row.set_delivery_d(o_row.entry_d());
      } else {
        ol_row.set_delivery_d(0);
      }
      ol_row.set_quantity(5);
      if (o_id < 2101) {
        ol_row.set_amount(0);
      } else {
        ol_row.set_amount(std::uniform_int_distribution<uint32_t>(1, 999999)(gen));
      }
      ol_row.set_dist_info(RandomAString(24, 24, gen));
      ol_row.SerializeToString(&ol_row_out);
      std::string ol_key = tpcc::OrderLineRowKey(w_id, d_id, o_id, ol_number);
      q.Push(std::make_pair(ol_key, ol_row_out));
    }
  }
}

void GenerateOrderTable(uint32_t num_warehouses, uint32_t c_load_ol_i_id, 
    Queue<std::pair<std::string, std::string>> &q) {
  for (uint32_t w_id = 1; w_id <= num_warehouses; ++w_id) {
    for (uint32_t d_id = 1; d_id <= 10; ++d_id) {
      GenerateOrderTableForWarehouseDistrict(w_id, d_id, c_load_ol_i_id, q);
    }
  }
}

void GenerateNewOrderTableForWarehouseDistrict(uint32_t w_id, uint32_t d_id,
    Queue<std::pair<std::string, std::string>> &q) {
  std::mt19937 gen;
  tpcc::NewOrderRow no_row;
  std::string no_row_out;
  for (uint32_t o_id = 2101; o_id <= 3000; ++o_id) {
    no_row.set_o_id(o_id);
    no_row.set_d_id(d_id);
    no_row.set_w_id(w_id);
    no_row.SerializeToString(&no_row_out);
    std::string no_key = tpcc::NewOrderRowKey(w_id, d_id, o_id);
    q.Push(std::make_pair(no_key, no_row_out));    
  }
}

void GenerateNewOrderTable(uint32_t num_warehouses,
    Queue<std::pair<std::string, std::string>> &q) {
  for (uint32_t w_id = 1; w_id <= num_warehouses; ++w_id) {
    for (uint32_t d_id = 1; d_id <= 10; ++d_id) {
      GenerateNewOrderTableForWarehouseDistrict(w_id, d_id, q);
    }
  }
}

DEFINE_int32(c_load_c_last, 0, "Run-time constant C used for generating C_LAST.");
//DEFINE_int32(c_load_c_id, 0, "Run-time constant C used for generating C_ID.");
DEFINE_int32(c_load_ol_i_id, 0, "Run-time constant C used for generating OL_I_ID.");
DEFINE_int32(num_warehouses, 1, "number of warehouses");
int main(int argc, char *argv[]) {
  gflags::SetUsageMessage(
           "generates a file containing key-value pairs of TPC-C table data\n");
	gflags::ParseCommandLineFlags(&argc, &argv, true);

  Queue<std::pair<std::string, std::string>> q(2e9);
  uint32_t time = std::time(0);
  std::cerr << "Generating " << FLAGS_num_warehouses << " warehouses." << std::endl;
  GenerateItemTable(q);
  GenerateWarehouseTable(FLAGS_num_warehouses, q);
  GenerateStockTable(FLAGS_num_warehouses, q);
  GenerateDistrictTable(FLAGS_num_warehouses, q);
  GenerateCustomerTable(FLAGS_num_warehouses, FLAGS_c_load_c_last, time, q);
  GenerateHistoryTable(FLAGS_num_warehouses, q);
  GenerateOrderTable(FLAGS_num_warehouses, FLAGS_c_load_ol_i_id, q);
  GenerateNewOrderTable(FLAGS_num_warehouses, q);
  std::pair<std::string, std::string> out;
  size_t count = 0;
//  while (!q.IsEmpty()) {
//    q.Pop(out);
//    count += WriteBytesToStream(&std::cout, out.first);
//    count += WriteBytesToStream(&std::cout, out.second);
//  }
  std::cerr << "Wrote " << count / 1024 / 1024 << "MB." << std::endl;
  return 0;
}
