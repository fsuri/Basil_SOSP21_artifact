#include "store/benchmark/async/tpcc/sync/stock_level.h"

#include <map>

#include "store/benchmark/async/tpcc/tpcc_utils.h"
namespace tpcc {

SyncStockLevel::SyncStockLevel(uint32_t w_id, uint32_t d_id, std::mt19937 &gen)
    : StockLevel(w_id, d_id, gen) {
}

SyncStockLevel::~SyncStockLevel() {
}

int SyncStockLevel::Execute(SyncClient &client) {
  std::string str;

  Debug("STOCK_LEVEL");
  Debug("Warehouse: %u", w_id);
  Debug("District: %u", d_id);
  
  client.Begin();

  std::string d_key = DistrictRowKey(w_id, d_id);
  client.Get(d_key, str, timeout);
  DistrictRow d_row;
  UW_ASSERT(d_row.ParseFromString(str));

  uint32_t next_o_id = d_row.next_o_id();
  Debug("Orders: %lu-%lu", next_o_id - 20, next_o_id - 1);

  std::map<uint32_t, StockRow> stockRows;
  for (size_t ol_o_id = next_o_id - 20; ol_o_id < next_o_id; ++ol_o_id) {
    Debug("Order %lu", ol_o_id);
    client.Get(OrderRowKey(w_id, d_id, ol_o_id), str, timeout);
    OrderRow o_row;
    UW_ASSERT(o_row.ParseFromString(str));
    Debug("  Order Lines: %lu", o_row.ol_cnt());

    for (size_t ol_number = 0; ol_number < o_row.ol_cnt(); ++ol_number) {
      Debug("    OL %u", ol_number);
      std::string ol_key = OrderLineRowKey(w_id, d_id, ol_o_id, ol_number);
      client.Get(ol_key, str, timeout);
      OrderLineRow ol_row;
      UW_ASSERT(ol_row.ParseFromString(str));
      Debug("      Item %lu", ol_row.i_id());

      if (stockRows.find(ol_row.i_id()) == stockRows.end()) {
        client.Get(StockRowKey(w_id, ol_row.i_id()), str, timeout);
        UW_ASSERT(stockRows[ol_row.i_id()].ParseFromString(str));
        Debug("Stock Item %lu quantity: %lu", ol_row.i_id(), stockRows[ol_row.i_id()].quantity());
      }
    }
  }
  Debug("COMMIT");
  return client.Commit(timeout);
}

} // namespace tpcc
