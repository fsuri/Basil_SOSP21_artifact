// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * store/benchmark/tpccClient.cc:
 *   Benchmarking client for tpcc.
 *
 **********************************************************************/

#include "lib/latency.h"
#include "lib/timeval.h"
#include "lib/tcptransport.h"
#include "store/common/truetime.h"
#include "store/common/stats.h"
#include "store/common/partitioner.h"
#include "store/common/frontend/sync_client.h"
#include "store/common/frontend/async_client.h"
#include "store/common/frontend/async_adapter_client.h"
#include "store/strongstore/client.h"
#include "store/weakstore/client.h"
#include "store/tapirstore/client.h"
#include "store/benchmark/async/bench_client.h"
#include "store/benchmark/async/common/key_selector.h"
#include "store/benchmark/async/common/uniform_key_selector.h"
#include "store/benchmark/async/retwis/retwis_client.h"
#include "store/benchmark/async/rw/rw_client.h"
#include "store/benchmark/async/tpcc/tpcc_client.h"
#include "store/mortystore/client.h"
#include "store/benchmark/async/smallbank/smallbank_client.h"
#include "store/janusstore/client.h"
#include "store/common/frontend/one_shot_client.h"
#include "store/common/frontend/async_one_shot_adapter_client.h"
#include "store/benchmark/async/common/zipf_key_selector.h"

#include <gflags/gflags.h>

#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

enum protomode_t {
	PROTO_UNKNOWN,
	PROTO_TAPIR,
	PROTO_WEAK,
	PROTO_STRONG,
  PROTO_JANUS,
  PROTO_MORTY
};

enum benchmode_t {
  BENCH_UNKNOWN,
  BENCH_RETWIS,
  BENCH_TPCC,
  BENCH_SMALLBANK_SYNC,
  BENCH_RW,
};

enum keysmode_t {
  KEYS_UNKNOWN,
  KEYS_UNIFORM,
  KEYS_ZIPF
};

/**
 * System settings.
 */
DEFINE_uint64(client_id, 0, "unique identifier for client");
DEFINE_string(config_path, "", "path to shard configuration file");
DEFINE_uint64(num_shards, 1, "number of shards in the system");
DEFINE_uint64(num_groups, 1, "number of replica groups in the system");
DEFINE_bool(tapir_sync_commit, true, "wait until commit phase completes before"
    " sending additional transactions (for TAPIR)");

const std::string protocol_args[] = {
	"txn-l",
  "txn-s",
  "qw",
  "occ",
  "lock",
  "span-occ",
  "span-lock",
  "janus",
  "morty"
};
const protomode_t protomodes[] {
  PROTO_TAPIR,
  PROTO_TAPIR,
  PROTO_WEAK,
  PROTO_STRONG,
  PROTO_STRONG,
  PROTO_STRONG,
  PROTO_STRONG,
  PROTO_JANUS,
  PROTO_MORTY
};
const strongstore::Mode strongmodes[] {
  strongstore::Mode::MODE_UNKNOWN,
  strongstore::Mode::MODE_UNKNOWN,
  strongstore::Mode::MODE_UNKNOWN,
  strongstore::Mode::MODE_OCC,
  strongstore::Mode::MODE_LOCK,
  strongstore::Mode::MODE_SPAN_OCC,
  strongstore::Mode::MODE_SPAN_LOCK,
  strongstore::Mode::MODE_UNKNOWN
};
static bool ValidateProtocolMode(const char* flagname,
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
DEFINE_string(protocol_mode, protocol_args[0],	"the mode of the protocol to"
    " use during this experiment");
DEFINE_validator(protocol_mode, &ValidateProtocolMode);

const std::string benchmark_args[] = {
	"retwis",
  "tpcc",
  "smallbank",
  "rw"
};
const benchmode_t benchmodes[] {
  BENCH_RETWIS,
  BENCH_TPCC,
  BENCH_SMALLBANK_SYNC,
  BENCH_RW
};
static bool ValidateBenchmark(const char* flagname, const std::string &value) {
  int n = sizeof(benchmark_args);
  for (int i = 0; i < n; ++i) {
    if (value == benchmark_args[i]) {
      return true;
    }
  }
  std::cerr << "Invalid value for --" << flagname << ": " << value << std::endl;
  return false;
}
DEFINE_string(benchmark, benchmark_args[0],	"the mode of the protocol to use"
    " during this experiment");
DEFINE_validator(benchmark, &ValidateBenchmark);

/**
 * Experiment settings.
 */
DEFINE_uint64(exp_duration, 30, "duration (in seconds) of experiment");
DEFINE_uint64(warmup_secs, 5, "time (in seconds) to warm up system before"
    " recording stats");
DEFINE_uint64(cooldown_secs, 5, "time (in seconds) to cool down system after"
    " recording stats");
DEFINE_uint64(tput_interval, 0, "time (in seconds) between throughput"
    " measurements");
DEFINE_uint64(num_clients, 1, "number of clients to run in this process");
DEFINE_uint64(num_requests, -1, "number of requests (transactions) per"
    " client");
DEFINE_int32(closest_replica, -1, "index of the replica closest to the client");
DEFINE_uint64(delay, 0, "simulated communication delay");
DEFINE_int32(clock_skew, 0, "difference between real clock and TrueTime");
DEFINE_int32(clock_error, 0, "maximum error for clock");
DEFINE_string(stats_file, "", "path to output stats file.");
DEFINE_int32(abort_backoff, 100, "sleep exponentially increasing amount after abort.");
DEFINE_bool(retry_aborted, true, "retry aborted transactions.");

/**
 * Retwis settings.
 */
DEFINE_string(keys_path, "", "path to file containing keys in the system"
		" (for retwis)");
DEFINE_uint64(num_keys, 0, "number of keys to generate (for retwis");

const std::string keys_args[] = {
	"uniform",
  "zipf"
};
const keysmode_t keysmodes[] {
  KEYS_UNIFORM,
  KEYS_ZIPF
};
static bool ValidateKeys(const char* flagname, const std::string &value) {
  int n = sizeof(keys_args);
  for (int i = 0; i < n; ++i) {
    if (value == keys_args[i]) {
      return true;
    }
  }
  std::cerr << "Invalid value for --" << flagname << ": " << value << std::endl;
  return false;
}
DEFINE_string(key_selector, keys_args[0],	"the distribution from which to "
    "select keys.");
DEFINE_validator(key_selector, &ValidateKeys);

DEFINE_double(zipf_coefficient, 0.5, "the coefficient of the zipf distribution "
    "for key selection.");

/**
 * RW settings.
 */
DEFINE_uint64(num_keys_txn, 1, "number of keys to read/write in each txn"
    " (for rw)");
// RW benchmark also uses same config parameters as Retwis.


/**
 * TPCC settings.
 */
DEFINE_int32(warehouse_per_shard, 1, "number of warehouses per shard"
		" (for tpcc)");
DEFINE_int32(clients_per_warehouse, 1, "number of clients per warehouse"
		" (for tpcc)");
DEFINE_int32(remote_item_milli_p, 0, "remote item milli p (for tpcc)");

DEFINE_int32(tpcc_num_warehouses, 1, "number of warehouses (for tpcc)");
DEFINE_int32(tpcc_w_id, 1, "home warehouse id for this client (for tpcc)");
DEFINE_int32(tpcc_C_c_id, 1, "C value for NURand() when selecting"
    " random customer id (for tpcc)");
DEFINE_int32(tpcc_C_c_last, 1, "C value for NURand() when selecting"
    " random customer last name (for tpcc)");
DEFINE_int32(tpcc_new_order_ratio, 45, "ratio of new_order transactions to other"
    " transaction types (for tpcc)");
DEFINE_int32(tpcc_delivery_ratio, 4, "ratio of delivery transactions to other"
    " transaction types (for tpcc)");
DEFINE_int32(tpcc_stock_level_ratio, 4, "ratio of stock_level transactions to other"
    " transaction types (for tpcc)");
DEFINE_int32(tpcc_payment_ratio, 43, "ratio of payment transactions to other"
    " transaction types (for tpcc)");
DEFINE_int32(tpcc_order_status_ratio, 4, "ratio of order_status transactions to other"
    " transaction types (for tpcc)");
DEFINE_bool(static_w_id, false, "force clients to use same w_id for each treansaction");

/**
 * Smallbank settings.
 */

DEFINE_int32(balance_ratio, 60, "percentage of balance transactions"
    " (for smallbank)");
DEFINE_int32(deposit_checking_ratio, 10, "percentage of deposit checking"
    " transactions (for smallbank)");
DEFINE_int32(transact_saving_ratio, 10, "percentage of transact saving"
    " transactions (for smallbank)");
DEFINE_int32(amalgamate_ratio, 10, "percentage of deposit checking"
    " transactions (for smallbank)");
DEFINE_int32(write_check_ratio, 10, "percentage of write check transactions"
    " (for smallbank)");
DEFINE_int32(num_hotspots, 1000, "# of hotspots (for smallbank)");
DEFINE_int32(num_customers, 18000, "# of customers (for smallbank)");
DEFINE_double(hotspot_probability, 90, "probability of ending in hotspot");
DEFINE_int32(timeout, 5000, "timeout in ms (for smallbank)");
DEFINE_string(customer_name_file_path, "smallbank_names", "path to file"
    " containing names to be loaded (for smallbank)");

DEFINE_LATENCY(op);

int main(int argc, char **argv) {
  gflags::SetUsageMessage(
           "executes transactions from various transactional workload\n"
"           benchmarks against various distributed replicated transaction\n"
"           processing systems.");
	gflags::ParseCommandLineFlags(&argc, &argv, true);

  // parse protocol and mode
  protomode_t mode = PROTO_UNKNOWN;
  strongstore::Mode strongmode = strongstore::Mode::MODE_UNKNOWN;
  int numProtoModes = sizeof(protocol_args);
  for (int i = 0; i < numProtoModes; ++i) {
    if (FLAGS_protocol_mode == protocol_args[i]) {
      mode = protomodes[i];
      strongmode = strongmodes[i];
      break;
    }
  }
  if (mode == PROTO_UNKNOWN || (mode == PROTO_STRONG
      && strongmode == strongstore::Mode::MODE_UNKNOWN)) {
    std::cerr << "Unknown protocol or unknown strongmode." << std::endl;
    return 1;
  }

  // parse benchmark
  benchmode_t benchMode = BENCH_UNKNOWN;
  int numBenchs = sizeof(benchmark_args);
  for (int i = 0; i < numBenchs; ++i) {
    if (FLAGS_benchmark == benchmark_args[i]) {
      benchMode = benchmodes[i];
      break;
    }
  }
  if (benchMode == BENCH_UNKNOWN) {
    std::cerr << "Unknown benchmark." << std::endl;
    return 1;
  }

  // parse key selector
  keysmode_t keySelectionMode = KEYS_UNKNOWN;
  int numKeySelectionModes = sizeof(keys_args);
  for (int i = 0; i < numKeySelectionModes; ++i) {
    if (FLAGS_key_selector == keys_args[i]) {
      keySelectionMode = keysmodes[i];
      break;
    }
  }
  if (keySelectionMode == KEYS_UNKNOWN) {
    std::cerr << "Unknown key selector." << std::endl;
    return 1;
  }

  // parse retwis settings
  std::vector<std::string> keys;
  if (benchMode == BENCH_RETWIS || benchMode == BENCH_RW) {
    if (FLAGS_keys_path.empty()) {
      if (FLAGS_num_keys > 0) {
        for (size_t i = 0; i < FLAGS_num_keys; ++i) {
          keys.push_back(std::to_string(i));
        }
      } else {
        std::cerr << "Specified neither keys file nor number of keys."
                  << std::endl;
        return 1;
      }
    } else {
      std::ifstream in;
      in.open(FLAGS_keys_path);
      if (!in) {
        std::cerr << "Could not read keys from: " << FLAGS_keys_path
                  << std::endl;
        return 1;
      }
      std::string key;
      while (std::getline(in, key)) {
        keys.push_back(key);
      }
      in.close();
    }
  }

  TCPTransport transport(0.0, 0.0, 0, false);

  std::vector<::AsyncClient *> asyncClients;
  std::vector<::SyncClient *> syncClients;
  std::vector<::Client *> clients;
  std::vector<::OneShotClient *> oneShotClients;
  std::vector<::BenchmarkClient *> benchClients;
  std::vector<std::thread *> threads;

  KeySelector *keySelector;
  switch (keySelectionMode) {
    case KEYS_UNIFORM:
      keySelector = new UniformKeySelector(keys);
      break;
    case KEYS_ZIPF:
      keySelector = new ZipfKeySelector(keys, FLAGS_zipf_coefficient);
      break;
    default:
      NOT_REACHABLE();
  }

  partitioner part;
  switch (benchMode) {
    case BENCH_TPCC:
      part = warehouse_partitioner;
      break;
    default:
      part = default_partitioner;
      break;
  }

	std::string latencyFile;
  std::string latencyRawFile;
  std::vector<uint64_t> latencies;
  std::atomic<size_t> clientsDone(0UL);

  int keysRead = 0;
  std::function<void()> jrcb = [&]() {
    keysRead++;
    if (clientsDone == FLAGS_num_clients && keysRead == FLAGS_num_keys) {
      transport.Stop();
    }
  };

  bench_done_callback bdcb = [&]() {
    ++clientsDone;
    if (clientsDone == FLAGS_num_clients) {
      Latency_t sum;
      _Latency_Init(&sum, "total");
      Stats total;
      for (unsigned int i = 0; i < benchClients.size(); i++) {
        Latency_Sum(&sum, &benchClients[i]->latency);
        total.Merge(benchClients[i]->GetStats());
      }
      Latency_Dump(&sum);
      if (latencyFile.size() > 0) {
        Latency_FlushTo(latencyFile.c_str());
      }

      latencyRawFile = latencyFile+".raw";
      std::ofstream rawFile(latencyRawFile.c_str(),
          std::ios::out | std::ios::binary);
      for (auto x : benchClients) {
        rawFile.write((char *)&x->latencies[0],
            (x->latencies.size()*sizeof(x->latencies[0])));
        if (!rawFile) {
          Warning("Failed to write raw latency output");
        }
      }

      if (FLAGS_stats_file.size() > 0) {
        total.ExportJSON(FLAGS_stats_file);
      }
      if (FLAGS_protocol_mode == "janus") {
        for (auto client : oneShotClients) {
          janusstore::Client *janus_client = (janusstore::Client*) client;
          for (auto key : keys) {
            janus_client->Read(key, jrcb);
          }
        }
      } else {
        transport.Stop();
      }
    }
  };
  for (size_t i = 0; i < FLAGS_num_clients; i++) {
    Client *client = nullptr;
    AsyncClient *asyncClient = nullptr;
    SyncClient *syncClient = nullptr;
    OneShotClient *oneShotClient = nullptr;

    switch (mode) {
      case PROTO_TAPIR: {
        client = new tapirstore::Client(FLAGS_config_path, FLAGS_num_shards,
            FLAGS_num_groups, FLAGS_closest_replica, &transport, part,
            FLAGS_tapir_sync_commit, TrueTime(FLAGS_clock_skew,
              FLAGS_clock_error));
        break;
      }
      case PROTO_JANUS: {
        oneShotClient = new janusstore::Client(FLAGS_config_path,
            FLAGS_num_shards, FLAGS_closest_replica, &transport);
        asyncClient = new AsyncOneShotAdapterClient(oneShotClient);
        break;
      }
      /*case MODE_WEAK: {
        protoClient = new weakstore::Client(configPath, nshards, closestReplica);
        break;
      }
      case MODE_STRONG: {
        protoClient = new strongstore::Client(strongmode, configPath, nshards,
            closestReplica, TrueTime(skew, error));
        break;
      }*/
      case PROTO_MORTY: {
        asyncClient = new mortystore::Client(FLAGS_config_path,
            (FLAGS_client_id << 3) | i, FLAGS_num_shards, FLAGS_num_groups,
            FLAGS_closest_replica, &transport, part);
        break;
      }
      default:
        NOT_REACHABLE();
    }

    switch (benchMode) {
      case BENCH_RETWIS:
      case BENCH_TPCC:
      case BENCH_RW:
        if (asyncClient == nullptr) {
          UW_ASSERT(client != nullptr);
          asyncClient = new AsyncAdapterClient(client);
        }
        break;
      case BENCH_SMALLBANK_SYNC:
        if (syncClient == nullptr) {
          UW_ASSERT(client != nullptr);
          syncClient = new SyncClient(client);
        }
        break;
      default:
        NOT_REACHABLE();
    }

	  BenchmarkClient *bench;
	  switch (benchMode) {
      case BENCH_RETWIS:
        UW_ASSERT(asyncClient != nullptr);
        bench = new retwis::RetwisClient(keySelector, *asyncClient, transport,
            (FLAGS_client_id << 3) | i,
            FLAGS_num_requests, FLAGS_exp_duration, FLAGS_delay,
            FLAGS_warmup_secs, FLAGS_cooldown_secs, FLAGS_tput_interval,
            FLAGS_abort_backoff, FLAGS_retry_aborted);
        break;
      case BENCH_TPCC:
        UW_ASSERT(asyncClient != nullptr);
        bench = new tpcc::TPCCClient(*asyncClient, transport,
            (FLAGS_client_id << 3) | i,
            FLAGS_num_requests, FLAGS_exp_duration, FLAGS_delay,
            FLAGS_warmup_secs, FLAGS_cooldown_secs, FLAGS_tput_interval,
            FLAGS_tpcc_num_warehouses, FLAGS_tpcc_w_id, FLAGS_tpcc_C_c_id,
            FLAGS_tpcc_C_c_last, FLAGS_tpcc_new_order_ratio,
            FLAGS_tpcc_delivery_ratio, FLAGS_tpcc_payment_ratio,
            FLAGS_tpcc_order_status_ratio, FLAGS_tpcc_stock_level_ratio,
            FLAGS_static_w_id, (FLAGS_client_id << 4) | i, FLAGS_abort_backoff,
            FLAGS_retry_aborted);
        break;
      case BENCH_SMALLBANK_SYNC:
        UW_ASSERT(syncClient != nullptr);
        bench = new smallbank::SmallbankClient(*syncClient, transport,
            (FLAGS_client_id << 3) | i,
            FLAGS_num_requests, FLAGS_exp_duration, FLAGS_delay,
            FLAGS_warmup_secs, FLAGS_cooldown_secs, FLAGS_tput_interval,
            FLAGS_abort_backoff, FLAGS_retry_aborted,
            FLAGS_timeout, FLAGS_balance_ratio, FLAGS_deposit_checking_ratio,
            FLAGS_transact_saving_ratio, FLAGS_amalgamate_ratio,
            FLAGS_num_hotspots, FLAGS_num_customers - FLAGS_num_hotspots, FLAGS_hotspot_probability,
            FLAGS_customer_name_file_path);
        break;
      case BENCH_RW:
        UW_ASSERT(asyncClient != nullptr);
        bench = new rw::RWClient(keySelector, FLAGS_num_keys_txn,
            *asyncClient, transport, (FLAGS_client_id << 3) | i,
            FLAGS_num_requests, FLAGS_exp_duration, FLAGS_delay,
            FLAGS_warmup_secs, FLAGS_cooldown_secs, FLAGS_tput_interval,
            FLAGS_abort_backoff, FLAGS_retry_aborted);
        break;
      default:
        NOT_REACHABLE();
    }

    switch (benchMode) {
      case BENCH_RETWIS:
      case BENCH_TPCC:
      case BENCH_RW:
        // async benchmarks
	      transport.Timer(0, [bench, bdcb]() { bench->Start(bdcb); });
        break;
      case BENCH_SMALLBANK_SYNC:
        threads.push_back(new std::thread([bench, bdcb](){
            bench->Start([](){});
            while (!bench->IsFullyDone()) {
              bench->StartLatency();
              bench->SendNext();
              bench->IncrementSent();
            }
            bdcb();
        }));
        break;
      default:
        NOT_REACHABLE();
    }

    if (asyncClient != nullptr) {
      asyncClients.push_back(asyncClient);
    }
    if (syncClient != nullptr) {
      syncClients.push_back(syncClient);
    }
    if (client != nullptr) {
      clients.push_back(client);
    }
    if (oneShotClient != nullptr) {
      oneShotClients.push_back(oneShotClient);
    }
    benchClients.push_back(bench);
  }

  transport.Run();
  for (auto i : threads) {
    i->join();
    delete i;
  }
  for (auto i : syncClients) {
    delete i;
  }
  for (auto i : oneShotClients) {
    delete i;
  }
  for (auto i : asyncClients) {
    delete i;
  }
  for (auto i : clients) {
    delete i;
  }
  for (auto i : benchClients) {
    delete i;
  }
	return 0;
}

