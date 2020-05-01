#include "store/benchmark/async/tpcc/sync/stock_level.h"

#include <map>

#include "store/benchmark/async/tpcc/tpcc_utils.h"
namespace tpcc {

SyncStockLevel::SyncStockLevel(uint32_t timeout, uint32_t w_id, uint32_t d_id,
    std::mt19937 &gen) : SyncTPCCTransaction(timeout),
    StockLevel(w_id, d_id, gen) {
}

SyncStockLevel::~SyncStockLevel() {
}

transaction_status_t SyncStockLevel::Execute(SyncClient &client) {
  std::string str;
  std::vector<std::string> strs;

  Debug("STOCK_LEVEL");
  Debug("Warehouse: %u", w_id);
  Debug("District: %u", d_id);
  
  client.Begin(timeout);

  std::string d_key = DistrictRowKey(w_id, d_id);
  client.Get(d_key, str, timeout);
  DistrictRow d_row;
  UW_ASSERT(d_row.ParseFromString(str));

  uint32_t next_o_id = d_row.next_o_id();
  Debug("Orders: %u-%u", next_o_id - 20, next_o_id - 1);

  for (size_t ol_o_id = next_o_id - 20; ol_o_id < next_o_id; ++ol_o_id) {
    Debug("Order %lu", ol_o_id);
    client.Get(OrderRowKey(w_id, d_id, ol_o_id), timeout);
  }

  client.Wait(strs);

  std::map<uint32_t, uint32_t> ol_cnts;
  for (uint32_t ol_o_id = next_o_id - 20; ol_o_id < next_o_id; ++ol_o_id) {
    if (strs[ol_o_id + 20 - next_o_id].empty()) {
      Debug("  Non-existent Order %u", ol_o_id);
      continue;
    }
    OrderRow o_row;
    UW_ASSERT(o_row.ParseFromString(strs[ol_o_id + 20 - next_o_id]));
    Debug("  Order Lines: %u", o_row.ol_cnt());

    ol_cnts[ol_o_id] = o_row.ol_cnt();
    for (size_t ol_number = 0; ol_number < o_row.ol_cnt(); ++ol_number) {
      Debug("    OL %lu", ol_number);
      std::string ol_key = OrderLineRowKey(w_id, d_id, ol_o_id, ol_number);
      client.Get(ol_key, timeout);
    }
  }

  strs.clear();
  client.Wait(strs);

  std::map<uint32_t, StockRow> stockRows;
  size_t strsIdx = 0;
  for (const auto order_cnt : ol_cnts) {
    Debug("Order %u", order_cnt.first);
    for (size_t ol_number = 0; ol_number < order_cnt.second; ++ol_number) {
      OrderLineRow ol_row;
      Debug("  OL %lu", ol_number);
      Debug("  Total OL index: %lu.", strsIdx);
      if (strs[strsIdx].empty()) {
        Debug("  Non-existent Order Line %lu", ol_number);
        continue;
      }
      UW_ASSERT(ol_row.ParseFromString(strs[strsIdx]));
      strsIdx++;
      Debug("      Item %d", ol_row.i_id());

      if (stockRows.find(ol_row.i_id()) == stockRows.end()) {
        client.Get(StockRowKey(w_id, ol_row.i_id()), timeout);
      }
    }
  }

  strs.clear();
  client.Wait(strs);

  Debug("COMMIT");
  return client.Commit(timeout);
}

} // namespace tpcc
