/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fsp@cs.cornell.edu>
 *                Matthew Burke <matthelb@cs.cornell.edu>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************************/
#include "store/benchmark/async/tpcc/sync/delivery.h"

#include "store/benchmark/async/tpcc/tpcc_utils.h"

namespace tpcc {

SyncDelivery::SyncDelivery(uint32_t timeout, uint32_t w_id, uint32_t d_id,
    std::mt19937 &gen)
    : SyncTPCCTransaction(timeout), Delivery(w_id, d_id, gen) {
}

SyncDelivery::~SyncDelivery() {
}

transaction_status_t SyncDelivery::Execute(SyncClient &client) {
  std::string str;
  std::vector<std::string> strs;

  Debug("DELIVERY");
  Debug("Warehouse: %u", w_id);
  Debug("District: %u", d_id);
  //std::cerr << "warehouse: " << w_id << std::endl;

  client.Begin(timeout);

  std::string eno_key = EarliestNewOrderRowKey(w_id, d_id);
  client.Get(eno_key, str, timeout);
  EarliestNewOrderRow eno_row;
  if (str.empty()) {
    // TODO: technically we're supposed to check each district in this warehouse
    return client.Commit(timeout);
  } else {
    UW_ASSERT(eno_row.ParseFromString(str));
  }
  uint32_t o_id = eno_row.o_id();
  Debug("  Earliest New Order: %u", o_id);

  eno_row.set_o_id(o_id + 1);
  eno_row.SerializeToString(&str);
  client.Put(eno_key, str, timeout);


  std::string o_key = OrderRowKey(w_id, d_id, o_id);
  client.Get(o_key, str, timeout);
  if (str.empty()) {
    // already delivered all orders for this warehouse
    return client.Commit(timeout);
  }

  client.Put(NewOrderRowKey(w_id, d_id, o_id), "", timeout); // delete
  OrderRow o_row;
  UW_ASSERT(o_row.ParseFromString(str));

  o_row.set_carrier_id(o_carrier_id);
  o_row.SerializeToString(&str);
  client.Put(o_key, str, timeout);
  Debug("  Carrier ID: %u", o_carrier_id);
  Debug("  Order Lines: %u", o_row.ol_cnt());

  for (size_t ol_number = 0; ol_number < o_row.ol_cnt(); ++ol_number) {
    client.Get(OrderLineRowKey(w_id, d_id, o_id, ol_number), timeout);
  }

  client.Wait(strs);

  int32_t total_amount = 0;
  for (size_t ol_number = 0; ol_number < o_row.ol_cnt(); ++ol_number) {
    Debug("    Order Line %lu", ol_number);
    OrderLineRow ol_row;
    UW_ASSERT(ol_row.ParseFromString(strs[ol_number]));
    Debug("      Amount: %i", ol_row.amount());
    total_amount += ol_row.amount();

    ol_row.set_delivery_d(ol_delivery_d);
    ol_row.SerializeToString(&str);
    client.Put(OrderLineRowKey(w_id, d_id, o_id, ol_number), str, timeout);
    Debug("      Delivery Date: %u", ol_delivery_d);
  }
  Debug("Total Amount: %i", total_amount);

  Debug("Customer: %u", o_row.c_id());
  std::string c_key = CustomerRowKey(w_id, d_id, o_row.c_id());
  client.Get(c_key, str, timeout);
  CustomerRow c_row;
  UW_ASSERT(c_row.ParseFromString(str));
  Debug("  Old Balance: %i", c_row.balance());

  c_row.set_balance(c_row.balance() + total_amount);
  Debug("  New Balance: %i", c_row.balance());
  c_row.set_delivery_cnt(c_row.delivery_cnt() + 1);
  c_row.SerializeToString(&str);
  client.Put(c_key, str, timeout);
  Debug("  Delivery Count: %u", c_row.delivery_cnt());

  Debug("COMMIT");
  return client.Commit(timeout);
}

} // namespace tpcc
