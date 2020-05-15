//
// Created by Janice Chan on 9/24/19.
//

#ifndef MORTY_TAPIR_SMALLBANK_CLIENT_H
#define MORTY_TAPIR_SMALLBANK_CLIENT_H

#include <gflags/gflags.h>

#include <algorithm>
#include <vector>

#include "lib/latency.h"
#include "lib/tcptransport.h"
#include "lib/timeval.h"
#include "store/benchmark/async/bench_client.h"
#include "store/benchmark/async/smallbank/smallbank_client.h"
#include "store/benchmark/async/smallbank/smallbank_transaction.h"
#include "store/benchmark/async/sync_transaction_bench_client.h"
#include "store/common/frontend/sync_client.h"
#include "store/common/truetime.h"
#include "store/tapirstore/client.h"

namespace smallbank {

constexpr int num_customers_ = 18000;

class SmallbankClient : public SyncTransactionBenchClient {
 public:
  SmallbankClient(SyncClient &client, Transport &transport, uint64_t id,
                  int numRequests, int expDuration, uint64_t delay,
                  int warmupSec, int cooldownSec, int tputInterval,
                  uint32_t abortBackoff, bool retryAborted, uint32_t maxBackoff,
                  uint32_t maxAttempts,
                  const uint32_t timeout, const uint32_t balance_ratio,
                  const uint32_t deposit_checking_ratio,
                  const uint32_t transact_saving_ratio,
                  const uint32_t amalgamate_ratio,
                  const uint32_t num_hotspot_keys,
                  const uint32_t num_non_hotspot_keys,
                  const double hotspot_probability,
                  const std::string &customer_name_file_path,
                  const std::string &latencyFilename = "");
  virtual ~SmallbankClient();
  virtual SyncTransaction *GetNextTransaction();
  virtual std::string GetLastOp() const;
  std::string GetCustomerKey(std::vector<std::string> keys,
                             uint32_t num_hotspot_keys,
                             uint32_t num_non_hotspot_keys,
                             double hotspot_probability);
  std::pair<string, string> GetCustomerKeyPair(std::vector<std::string> keys,
                                               uint32_t num_hotspot_keys,
                                               uint32_t num_non_hotspot_keys,
                                               double hotspot_probability);
  void SetCustomerKeys(std::vector<std::string> keys);

 protected:
 private:
  uint32_t timeout_;
  uint32_t balance_ratio_;
  uint32_t deposit_checking_ratio_;
  uint32_t transact_saving_ratio_;
  uint32_t amalgamate_ratio_;
  uint32_t num_hotspot_keys_;
  uint32_t num_non_hotspot_keys_;
  double hotspot_probability_;
  std::vector<std::string> all_keys_;
  std::string last_op_;
};

}  // namespace smallbank

#endif  // MORTY_TAPIR_SMALLBANK_CLIENT_H
