//
// Created by Janice Chan on 9/24/19.
//

#include "store/benchmark/async/smallbank/smallbank_client.h"

#include <gflags/gflags.h>

#include <algorithm>
#include <random>
#include <vector>

#include "lib/latency.h"
#include "lib/tcptransport.h"
#include "lib/timeval.h"
#include "store/benchmark/async/bench_client.h"
#include "store/benchmark/async/smallbank/amalgamate.h"
#include "store/benchmark/async/smallbank/bal.h"
#include "store/benchmark/async/smallbank/deposit.h"
#include "store/benchmark/async/smallbank/smallbank_transaction.h"
#include "store/benchmark/async/smallbank/transact.h"
#include "store/benchmark/async/smallbank/write_check.h"
#include "store/common/frontend/sync_client.h"
#include "store/common/truetime.h"
#include "store/tapirstore/client.h"

namespace smallbank {

SmallbankClient::SmallbankClient(
    SyncClient &client, Transport &transport, uint64_t id,
    int numRequests, int expDuration, uint64_t delay, int warmupSec,
    int cooldownSec, int tputInterval, uint32_t abortBackoff, bool retryAborted,
    uint32_t maxBackoff, uint32_t maxAttempts,
    const uint32_t timeout, const uint32_t balance_ratio,
    const uint32_t deposit_checking_ratio, const uint32_t transact_saving_ratio,
    const uint32_t amalgamate_ratio, const uint32_t num_hotspot_keys,
    const uint32_t num_non_hotspot_keys, const double hotspot_probability,
    const std::string &customer_name_file_path,
    const std::string &latencyFilename)
    : SyncTransactionBenchClient(client, transport, id, numRequests,
                                 expDuration, delay, warmupSec, cooldownSec,
                                 tputInterval, abortBackoff, retryAborted, maxBackoff, maxAttempts, timeout,
                                 latencyFilename),
      timeout_(timeout),
      balance_ratio_(balance_ratio),
      deposit_checking_ratio_(deposit_checking_ratio),
      transact_saving_ratio_(transact_saving_ratio),
      amalgamate_ratio_(amalgamate_ratio),
      num_hotspot_keys_(num_hotspot_keys),  // first `num_hotpost_keys_` in
                                            // `all_keys_` is the hotspot
      num_non_hotspot_keys_(num_non_hotspot_keys),
      hotspot_probability_(hotspot_probability) { 
  std::string str;
  std::ifstream file(customer_name_file_path);
  while (getline(file, str, ',')) {
    all_keys_.push_back(str);
  }
}

SmallbankClient::~SmallbankClient() {}

void SmallbankClient::SetCustomerKeys(std::vector<std::string> keys) {
  all_keys_ = keys;
}

SyncTransaction *SmallbankClient::GetNextTransaction() {
  std::uniform_int_distribution<int> dist(0, 99);
  int ttype = dist(GetRand());
  int balanceThreshold = balance_ratio_;
  int depositThreshold = balanceThreshold + deposit_checking_ratio_;
  int transactThreshold = depositThreshold + transact_saving_ratio_;
  int amalgamateThreshold = transactThreshold + amalgamate_ratio_;
  // Ranges for random params for transactions based on
  // https://github.com/microsoft/CCF/blob/master/samples/apps/smallbank/clients/small_bank_client.cpp
  if (ttype < balanceThreshold) {
    last_op_ = "balance";
    return new Bal(GetCustomerKey(all_keys_, num_hotspot_keys_,
                                  num_non_hotspot_keys_, hotspot_probability_),
                   timeout_);
  }
  if (ttype < depositThreshold) {
    last_op_ = "deposit";
    return new DepositChecking(
        GetCustomerKey(all_keys_, num_hotspot_keys_,
                       num_non_hotspot_keys_, hotspot_probability_),
        GetRand()() % 50 + 1, timeout_);
  }
  if (ttype < transactThreshold) {
    last_op_ = "transact";
    return new TransactSaving(
        GetCustomerKey(all_keys_, num_hotspot_keys_,
                       num_non_hotspot_keys_, hotspot_probability_),
        GetRand()() % 101 - 50, timeout_);
  }
  if (ttype < amalgamateThreshold) {
    last_op_ = "amalgamate";
    std::pair<string, string> keyPair =
        GetCustomerKeyPair(all_keys_, num_hotspot_keys_,
                           num_non_hotspot_keys_, hotspot_probability_);
    return new Amalgamate(keyPair.first, keyPair.second, timeout_);
  }
  last_op_ = "write_check";
  return new WriteCheck(
      GetCustomerKey(all_keys_, num_hotspot_keys_, num_non_hotspot_keys_,
                     hotspot_probability_),
      GetRand()() % 50, timeout_);
}

std::string SmallbankClient::GetLastOp() const { return last_op_; }

std::string SmallbankClient::GetCustomerKey(std::vector<std::string> all_keys,
                                            uint32_t num_hotspot_keys,
                                            uint32_t num_non_hotspot_keys,
                                            double hotspot_probability) {
  std::uniform_int_distribution<int> hotspotDistribution(
      0, num_hotspot_keys + num_non_hotspot_keys - 1);
  bool inHotspot =
      hotspotDistribution(GetRand()) <
      hotspot_probability * (num_hotspot_keys + num_non_hotspot_keys);
  int range = inHotspot ? num_hotspot_keys : num_non_hotspot_keys;
  std::uniform_int_distribution<int> relevantKeyDistribution(0, range - 1);
  int offset = inHotspot ? 0 : num_hotspot_keys;
  return all_keys[relevantKeyDistribution(GetRand()) + offset];
};

std::pair<std::string, std::string> SmallbankClient::GetCustomerKeyPair(
    std::vector<std::string> all_keys,
    uint32_t num_hotspot_keys, uint32_t num_non_hotspot_keys,
    double hotspot_probability) {
  std::uniform_int_distribution<int> hotspotDistribution(
      0, num_hotspot_keys + num_non_hotspot_keys - 1);
  bool inHotspot =
      hotspotDistribution(GetRand()) <
      hotspot_probability * (num_hotspot_keys + num_non_hotspot_keys);
  int range = inHotspot ? num_hotspot_keys : num_non_hotspot_keys;
  std::uniform_int_distribution<int> relevantKey1Distribution(0, range - 1);
  int offset = inHotspot ? 0 : num_hotspot_keys;
  int key1Idx = relevantKey1Distribution(GetRand()) + offset;
  string key1 = all_keys[key1Idx];
  std::swap(all_keys[key1Idx], all_keys[range + offset - 1]);
  std::uniform_int_distribution<int> relevantKey2Distribution(0, range - 2);
  string key2 = all_keys[relevantKey2Distribution(GetRand()) + offset];
  return std::make_pair(key1, key2);
};

}  // namespace smallbank
