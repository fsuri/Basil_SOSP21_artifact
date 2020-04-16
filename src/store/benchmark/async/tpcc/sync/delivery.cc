#include "store/benchmark/async/tpcc/sync/delivery.h"

#include "store/benchmark/async/tpcc/tpcc_utils.h"

namespace tpcc {

SyncDelivery::SyncDelivery(uint32_t w_id, uint32_t d_id, std::mt19937 &gen)
    : Delivery(w_id, d_id, gen) {
}

SyncDelivery::~SyncDelivery() {
}

int SyncDelivery::Execute(SyncClient &client) {
  std::string str;

  client.Begin();
  
  std::string eno_key = EarliestNewOrderRowKey(w_id, d_id);
  client.Get(eno_key, str, timeout);
  EarliestNewOrderRow eno_row;
  // TODO: update tpcc generator w/ EarliestNewOrder table
  if (str.empty()) {
    eno_row.set_w_id(w_id);
    eno_row.set_d_id(d_id);
    eno_row.set_o_id(2101UL); // 
  } else {
    UW_ASSERT(eno_row.ParseFromString(str));
  }

  eno_row.set_o_id(eno_row.o_id() + 1);
  eno_row.SerializeToString(&str);
  client.Put(eno_key, str, timeout);

  client.Put(NewOrderRowKey(w_id, d_id, eno_row.o_id()), "", timeout); // delete

  std::string o_key = OrderRowKey(w_id, d_id, eno_row.o_id());
  client.Get(o_key, str, timeout);
  OrderRow o_row;
  UW_ASSERT(o_row.ParseFromString(str));

  o_row.set_carrier_id(o_carrier_id);
  o_row.SerializeToString(&str);
  client.Put(o_key, str, timeout);

  size_t total_amount = 0;
  for (size_t ol_number = 0; ol_number < o_row.ol_cnt(); ++ol_number) {
    std::string ol_key = OrderLineRowKey(w_id, d_id, eno_row.o_id(), ol_number);
    client.Get(ol_key, str, timeout);
    OrderLineRow ol_row;
    UW_ASSERT(ol_row.ParseFromString(str));
    total_amount += ol_row.amount();

    ol_row.set_delivery_d(ol_delivery_d);
    ol_row.SerializeToString(&str);
    client.Put(ol_key, str, timeout);
  }

  std::string c_key = CustomerRowKey(w_id, d_id, o_row.c_id());
  client.Get(c_key, str, timeout);
  CustomerRow c_row;
  UW_ASSERT(c_row.ParseFromString(str));

  c_row.set_balance(c_row.balance() + total_amount);
  c_row.set_delivery_cnt(c_row.delivery_cnt() + 1);
  c_row.SerializeToString(&str);
  client.Put(c_key, str, timeout);

  return client.Commit(timeout);
}

} // namespace tpcc
