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

#include "lib/latency.h"
#include "lib/timeval.h"
#include "lib/tcptransport.h"
#include "store/common/truetime.h"
#include "store/common/stats.h"
#include "store/common/partitioner.h"
#include "store/common/frontend/async_client.h"
#include "store/common/frontend/async_adapter_client.h"
#include "store/strongstore/client.h"
#include "store/weakstore/client.h"
#include "store/tapirstore/client.h"
#include "store/benchmark/async/bench_client.h"
#include "store/benchmark/async/common/key_selector.h"
#include "store/benchmark/async/common/uniform_key_selector.h"
#include "store/benchmark/async/retwis/retwis_client.h"
#include "store/benchmark/async/tpcc/tpcc_client.h"


#include <vector>
#include <algorithm>

enum protomode_t {
    PROTO_TAPIR,
    PROTO_UNKNOWN
};

/**
 * System settings.
 */
DEFINE_string(config_prefix,
"smallbank.config", "prefix of path to shard configuration file");
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
1000, "# of hotspots (smallbank)");
DEFINE_uint32(num_customers,
18000, "# of customers (smallbank)");
//TODO change to 5000 or 10000
DEFINE_uint32(timeout,
0, "timeout in ms (smallbank)");
DEFINE_string(customer_name_file_path, "smallbank_names", "path to file containing names to be loaded");

void loadKeys(std::string keys[], std::string customer_name_file_path) {
    std::string str;
    std::ifstream file(customer_name_file_path);
    int i=0;
    while (getline(file, str, ',')) {
        keys[i] = str;
        i+=1;
    }
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

    std::cout<<"clients " << FLAGS_num_clients <<std::endl;
    for (size_t i = 0; i < FLAGS_num_clients; i++) {
        std::cout<<"client " << i<<std::endl;
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
//        std::cout<<"loading all keys"<<std::endl;
        std::string allKeys[FLAGS_num_customers];
        loadKeys(allKeys, FLAGS_customer_name_file_path);
//        std::cout<<"loaded all keys" <<std::endl;
//        std::cout<<allKeys[0]<< " to  " << allKeys[FLAGS_num_customers-1] <<std::endl;
        smallbank::SmallBankClient *bench = new smallbank::SmallBankClient(client, FLAGS_timeout, FLAGS_balance_ratio,
                                                                           FLAGS_deposit_checking_ratio,
                                                                           FLAGS_transact_saving_ratio, FLAGS_amalgamate_ratio,
                                                                           FLAGS_num_hotspots,
                                                                           FLAGS_num_customers - FLAGS_num_hotspots,
                                                                           allKeys);
        std::cout<<"created client " << i<<std::endl;
        bench->startBenchmark(FLAGS_duration, FLAGS_rampup);
    }
    return 0;
}