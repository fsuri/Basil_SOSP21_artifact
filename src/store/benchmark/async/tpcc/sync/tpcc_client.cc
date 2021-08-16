/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fs435@cornell.edu>
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
#include "store/benchmark/async/tpcc/sync/tpcc_client.h"

#include <random>

#include "store/benchmark/async/tpcc/sync/new_order.h"
#include "store/benchmark/async/tpcc/sync/payment.h"
#include "store/benchmark/async/tpcc/sync/order_status.h"
#include "store/benchmark/async/tpcc/sync/stock_level.h"
#include "store/benchmark/async/tpcc/sync/delivery.h"

namespace tpcc {

SyncTPCCClient::SyncTPCCClient(SyncClient &client, Transport &transport,
    uint64_t seed, int numRequests, int expDuration, uint64_t delay, int warmupSec,
    int cooldownSec, int tputInterval,  uint32_t num_warehouses, uint32_t w_id,
    uint32_t C_c_id, uint32_t C_c_last, uint32_t new_order_ratio,
    uint32_t delivery_ratio, uint32_t payment_ratio, uint32_t order_status_ratio,
    uint32_t stock_level_ratio, bool static_w_id,
    uint32_t abortBackoff, bool retryAborted, uint32_t maxBackoff, uint32_t maxAttempts, uint32_t timeout,
    const std::string &latencyFilename) :
      SyncTransactionBenchClient(client, transport, seed, numRequests,
        expDuration, delay, warmupSec, cooldownSec, tputInterval, abortBackoff,
        retryAborted, maxBackoff, maxAttempts, timeout, latencyFilename),
      TPCCClient(num_warehouses, w_id, C_c_id, C_c_last, new_order_ratio,
        delivery_ratio, payment_ratio, order_status_ratio, stock_level_ratio,
        static_w_id, GetRand()) {
  stockLevelDId = std::uniform_int_distribution<uint32_t>(1, 10)(GetRand());
}

SyncTPCCClient::~SyncTPCCClient() {
}

SyncTransaction* SyncTPCCClient::GetNextTransaction() {
  uint32_t wid, did;
  TPCCTransactionType ttype = TPCCClient::GetNextTransaction(&wid, &did, GetRand());

  switch (ttype) {
    case TXN_NEW_ORDER:
      return new SyncNewOrder(GetTimeout(), wid, C_c_id, num_warehouses, GetRand());
    case TXN_PAYMENT:
      return new SyncPayment(GetTimeout(), wid, C_c_last, C_c_id, num_warehouses, GetRand());
    case TXN_ORDER_STATUS:
      return new SyncOrderStatus(GetTimeout(), wid, C_c_last, C_c_id, GetRand());
    case TXN_STOCK_LEVEL:
      return new SyncStockLevel(GetTimeout(), wid, did, GetRand());
    case TXN_DELIVERY:
      return new SyncDelivery(GetTimeout(), wid, did, GetRand());
    default:
      NOT_REACHABLE();
  }
}

std::string SyncTPCCClient::GetLastOp() const {
  return lastOp;
}


} //namespace tpcc
