//
// Created by Janice Chan on 9/24/19.
//

#include "lib/tcptransport.h"
#include "store/benchmark/async/bench_client.h"
#include "store/benchmark/async/smallbank/smallbank_client.h"
#include "store/benchmark/async/smallbank/smallbank_transaction.h"
#include "store/common/frontend/sync_client.h"
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

    enum protomode_t {
        PROTO_TAPIR,
        PROTO_UNKNOWN
    };

/**
 * System settings.
 */
    DEFINE_string(config_prefix,
    "", "prefix of path to shard configuration file");
    DEFINE_uint64(num_shards,
    1, "number of shards in the system");
    DEFINE_uint64(num_groups,
    1, "number of replica groups in the system");
    DEFINE_bool(tapir_sync_commit,
    true, "wait until commit phase completes before"
    " sending additional transactions (for TAPIR)");

    const std::string protocol_args[] = {
            "txn"
    };

    const protomode_t protomodes[]{
            PROTO_TAPIR
    };

    static bool ValidateProtocolMode(const char *flagname,
                                     const std::string &value) {
        int n = sizeof(protocol_args);
        for (int i = 0; i < n; ++i) {
            if (value == protocol_args[i]) {
                return true;
            }
        }
        std::cerr << "Invalid value for --" << flagname << ": " << value << std::endl;
        return false;
    }

    DEFINE_string(protocol_mode, protocol_args[0],
    "the mode of the protocol to use"
    " during this experiment");
    DEFINE_validator(protocol_mode, &ValidateProtocolMode
    );

/**
 * Experiment settings.
 */
    DEFINE_uint64(num_clients,
    1, "number of clients to run in this process");
    DEFINE_int32(closest_replica,
    -1, "index of the replica closest to the client");
    DEFINE_int32(clock_skew,
    0, "difference between real clock and TrueTime");
    DEFINE_int32(clock_error,
    0, "maximum error for clock");

/**
 * Smallbank settings.
 */
    DEFINE_uint32(duration,
    60, "seconds to run (smallbank)");
    DEFINE_uint32(rampup,
    30, "rampup period in seconds (smallbank)");
    DEFINE_uint32(balance_ratio,
    60, "percentage of balance transactions (smallbank)");
    DEFINE_uint32(deposit_checking_ratio,
    10, "percentage of deposit checking transactions (smallbank)");
    DEFINE_uint32(transact_saving_ratio,
    10, "percentage of transact saving transactions (smallbank)");
    DEFINE_uint32(amalgamate_ratio,
    10, "percentage of deposit checking transactions (smallbank)");
    DEFINE_uint32(write_check_ratio,
    10, "percentage of write check transactions (smallbank)");
    DEFINE_uint32(num_hotspots,
    0, "# of hotspots (smallbank)");

    DEFINE_uint32(timeout,
    0, "timeout in ms (smallbank)");

    void loadKeys(std::string keys[]) {
        //TODO load generated file
    }

    int main(int argc, char **argv) {
        gflags::SetUsageMessage(
                "executes transactions from SmallBank workload\n"
                "           benchmarks against various distributed replicated transaction\n"
                "           processing systems.");
        gflags::ParseCommandLineFlags(&argc, &argv, true);

        // input validation
        if (FLAGS_balance_ratio + FLAGS_deposit_checking_ratio + FLAGS_transact_saving_ratio + FLAGS_amalgamate_ratio +
            FLAGS_write_check_ratio != 100 || FLAGS_num_hotspots < 2 ) {
            return 1;
        }

        // parse protocol
        protomode_t mode = PROTO_UNKNOWN;
        int numProtoModes = sizeof(protocol_args);
        for (int i = 0; i < numProtoModes; ++i) {
            if (FLAGS_protocol_mode == protocol_args[i]) {
                mode = protomodes[i];
                break;
            }
        }
        if (mode == PROTO_UNKNOWN) {
            std::cerr << "Unknown protocol." << std::endl;
            return 1;
        }

        TCPTransport transport(0.0, 0.0, 0, false);

        partitioner part = default_partitioner;

        for (size_t i = 0; i < FLAGS_num_clients; i++) {
            SyncClient *client;
            switch (mode) {
                case PROTO_TAPIR: {
                    client = new SyncClient(new tapirstore::Client(
                            FLAGS_config_prefix, FLAGS_num_shards, FLAGS_num_groups,
                            FLAGS_closest_replica, &transport, part, FLAGS_tapir_sync_commit,
                            TrueTime(FLAGS_clock_skew, FLAGS_clock_error)));
                    break;
                }
                default:
                    NOT_REACHABLE();
            }

            std::string allKeys[num_customers_];
            loadKeys(allKeys);
            SmallBankClient *bench = new smallbank::SmallBankClient(client, FLAGS_timeout, FLAGS_balance_ratio,
                                                                    FLAGS_deposit_checking_ratio,
                                                                    FLAGS_transact_saving_ratio, FLAGS_amalgamate_ratio,
                                                                    FLAGS_num_hotspots,
                                                                    num_customers_ - FLAGS_num_hotspots,
                                                                    allKeys);
            bench->startBenchmark(FLAGS_duration);
        }
        return 0;
    }

}