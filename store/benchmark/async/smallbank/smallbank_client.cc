//
// Created by Janice Chan on 9/24/19.
//

#include "lib/tcptransport.h"
#include "lib/latency.h"
#include "lib/timeval.h"
#include "store/benchmark/async/bench_client.h"
#include "store/benchmark/async/smallbank/smallbank_client.h"
#include "store/benchmark/async/smallbank/smallbank_transaction.h"
#include "store/common/frontend/sync_client.h"
#include "store/common/truetime.h"
#include "store/tapirstore/client.h"
#include <gflags/gflags.h>

#include <vector>
#include <algorithm>

namespace smallbank {

    SmallbankClient::SmallbankClient(SyncClient &client, Transport &transport, int numRequests, int expDuration,
                                     uint64_t delay, int warmupSec, int cooldownSec, int tputInterval,
                                     uint32_t abortBackoff, bool retryAborted,
                                     const uint32_t timeout,
                                     const uint32_t balance_ratio, const uint32_t deposit_checking_ratio,
                                     const uint32_t transact_saving_ratio, const uint32_t amalgamate_ratio,
                                     const uint32_t num_hotspot_keys, const uint32_t num_non_hotspot_keys,
                                     const std::string &customer_name_file_path,
                                     const std::string &latencyFilename) : SyncTransactionBenchClient(client,
                                                                                                      transport, numRequests, expDuration,
                                                                                                      delay, warmupSec, cooldownSec, tputInterval, abortBackoff,
                                                                                                      retryAborted, latencyFilename), timeout_(timeout),
                                                                           balance_ratio_(balance_ratio),
                                                                           deposit_checking_ratio_(deposit_checking_ratio),
                                                                           transact_saving_ratio_(transact_saving_ratio),
                                                                           amalgamate_ratio_(amalgamate_ratio),
                                                                           num_hotspot_keys_(num_hotspot_keys), // first `num_hotpost_keys_` in `all_keys_` is the hotspot
                                                                           num_non_hotspot_keys_(num_non_hotspot_keys) {
        std::string str;
        std::ifstream file(customer_name_file_path);
        int i=0;
        while (getline(file, str, ',')) {
            all_keys_.push_back(str);
        }
    }

    SyncTransaction *SmallbankClient::GetNextTransaction() {
        int ttype = rand() % 100;

        // Ranges for random params for transactions based on
        // https://github.com/microsoft/CCF/blob/master/samples/apps/smallbank/clients/small_bank_client.cpp
        if (ttype < balance_ratio_) {
            return new SmallbankTransaction(BALANCE, getCustomerKey(), "", timeout_);
        } else if (ttype < balance_ratio_ + deposit_checking_ratio_) {
            return new SmallbankTransaction(DEPOSIT, getCustomerKey(), "", timeout_);
        } else if (ttype < balance_ratio_ + deposit_checking_ratio_ + transact_saving_ratio_) {
            return new SmallbankTransaction(TRANSACT, getCustomerKey(), "", timeout_);
        } else if (ttype < balance_ratio_ + deposit_checking_ratio_ + transact_saving_ratio_ + amalgamate_ratio_) {
            std::pair <string, string> keyPair = getCustomerKeyPair();
            return new SmallbankTransaction(BALANCE, keyPair.first, keyPair.second, timeout_);
        } else {
            return new SmallbankTransaction(WRITE_CHECK, getCustomerKey(), "", timeout_);
        }
    }

    std::string SmallbankClient::GetLastOp() const {
        return "";
    }

    std::string SmallbankClient::getCustomerKey() {
        bool inHotspot = (rand() % (num_hotspot_keys_ + num_non_hotspot_keys_)) < num_hotspot_keys_;
        if (inHotspot) {
            std::string key = all_keys_[rand() % num_hotspot_keys_];
            return key;
        }

        std::string key2 = all_keys_[rand() % num_non_hotspot_keys_ + num_hotspot_keys_];
        return key2;
    };

    std::pair <std::string, std::string> SmallbankClient::getCustomerKeyPair() {
        bool inHotspot = rand() % (num_hotspot_keys_ + num_non_hotspot_keys_) < num_hotspot_keys_;
        if (inHotspot) {
            int key1Idx = rand() % num_hotspot_keys_;
            string key1 = all_keys_[key1Idx];
            all_keys_[key1Idx] = all_keys_[num_hotspot_keys_ - 1];

            int key2Idx = rand() % (num_hotspot_keys_ - 1);
            string key2 = all_keys_[key2Idx];

            all_keys_[key1Idx] = key1;
            return std::make_pair(key1, key2);
        }
        // non hotspot keys start at index `num_hotspot_keys_`
        int key1Idx = rand() % num_non_hotspot_keys_ + num_hotspot_keys_;
        string key1 = all_keys_[key1Idx];
        all_keys_[key1Idx] = all_keys_[num_non_hotspot_keys_ + num_hotspot_keys_ - 1];

        int key2Idx = rand() % (num_non_hotspot_keys_ - 1) + num_hotspot_keys_;
        string key2 = all_keys_[key2Idx];

        all_keys_[key1Idx] = key1;
        return std::make_pair(key1, key2);
    };

}