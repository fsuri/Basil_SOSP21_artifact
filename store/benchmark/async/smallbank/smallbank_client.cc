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
    SmallBankClient::SmallBankClient(SyncClient *client,
                                     const uint32_t timeout, const uint32_t balance_ratio,
                                     const uint32_t deposit_checking_ratio,
                                     const uint32_t transact_saving_ratio, const uint32_t amalgamate_ratio,
                                     const uint32_t num_hotspot_keys,
                                     const uint32_t num_non_hotspot_keys, std::string all_keys[num_customers_]) :

            transaction_(SmallBankTransaction(client, timeout)), balance_ratio_(balance_ratio),
            deposit_checking_ratio_(deposit_checking_ratio),
            transact_saving_ratio_(transact_saving_ratio), amalgamate_ratio_(amalgamate_ratio),
            num_hotspot_keys_(num_hotspot_keys), // first `num_hotpost_keys_` in `all_keys_` is the hotspot
            num_non_hotspot_keys_(num_non_hotspot_keys) {
        std::copy(all_keys, all_keys+0, all_keys_);
    }

    void SmallBankClient::startBenchmark(const uint32_t duration, const uint32_t rampup) {
        struct timeval t0, t1, t2;
        int nTransactions = 0; // Number of transactions attempted.
        int ttype; // Transaction type
        bool status;

        gettimeofday(&t0, NULL);
        srand(t0.tv_sec + t0.tv_usec);

        while (1) {
            gettimeofday(&t1, NULL);
            status = true;
            // Decide which type of smallbank transaction it is going to be.
            ttype = rand() % 100;

            // Ranges for random params for transactions based on
            // https://github.com/microsoft/CCF/blob/master/samples/apps/smallbank/clients/small_bank_client.cpp
            if (ttype < balance_ratio_) {
                if (!transaction_.Bal(getCustomerKey()).second) {
                    status = false;
                }
                ttype = 1;
            } else if (ttype < balance_ratio_ + deposit_checking_ratio_) {
                if (!transaction_.DepositChecking(getCustomerKey(), rand() % 50 + 1)) {
                    status = false;
                }
                ttype = 2;
            } else if (ttype < balance_ratio_ + deposit_checking_ratio_ + transact_saving_ratio_) {
                if (!transaction_.TransactSaving(getCustomerKey(), rand() % 101 - 50)) {
                    status = false;
                }
                ttype = 3;
            } else if (ttype < balance_ratio_ + deposit_checking_ratio_ + transact_saving_ratio_ + amalgamate_ratio_) {
                std::pair <string, string> keyPair = getCustomerKeyPair();
                if (!transaction_.Amalgamate(keyPair.first, keyPair.second)) {
                    status = false;
                }
                ttype = 4;
            } else {
                if (!transaction_.WriteCheck(getCustomerKey(), rand() % 50)) {
                    status = false;
                }
                ttype = 5;
            }
            gettimeofday(&t2, NULL);
            if (((t2.tv_sec - t0.tv_sec) * 1000000 + (t2.tv_usec - t0.tv_usec)) < rampup * 1000000) {
                continue;
            }
            long latency = (t2.tv_sec - t1.tv_sec) * 1000000 + (t2.tv_usec - t1.tv_usec);
            fprintf(stderr, "%d %ld.%06ld %ld.%06ld %ld %d %d", ++nTransactions, t1.tv_sec,
                    t1.tv_usec, t2.tv_sec, t2.tv_usec, latency, status?1:0, ttype);
            fprintf(stderr, "\n");
            if (((t2.tv_sec - t0.tv_sec) * 1000000 + (t2.tv_usec - t0.tv_usec)) > (duration + rampup) * 1000000)
                break;
        }

        fprintf(stderr, "# Client exiting..\n");
    }

    std::string SmallBankClient::getCustomerKey() {
        bool inHotspot = rand() % (num_hotspot_keys_ + num_non_hotspot_keys_) < num_hotspot_keys_;
        if (inHotspot) {
            return all_keys_[rand() % num_hotspot_keys_];
        }
        return all_keys_[rand() % num_non_hotspot_keys_ + num_hotspot_keys_];
    };

    std::pair <std::string, std::string> SmallBankClient::getCustomerKeyPair() {
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

