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
#include "store/common/frontend/client.h"
#include "store/strongstore/client.h"
#include "store/weakstore/client.h"
#include "store/tapirstore/client.h"
#include "store/benchmark/async/bench_client.h"
#include "store/benchmark/async/common/key_selector.h"
#include "store/benchmark/async/common/uniform_key_selector.h"
#include "store/benchmark/async/retwis/retwis_client.h"
#include "store/benchmark/async/tpcc/tpcc_client.h"

#include <gflags/gflags.h>

#include <vector>
#include <algorithm>

enum protomode_t {
	PROTO_UNKNOWN,
	PROTO_TAPIR,
	PROTO_WEAK,
	PROTO_STRONG
};

enum benchmode_t {
  BENCH_UNKNOWN,
  BENCH_RETWIS,
  BENCH_TPCC
};

/**
 * System settings."
 */
DEFINE_uint64(client_id, 0, "unique identifier for client");
DEFINE_string(config_path, "", "prefix of path to shard configuration file");
DEFINE_uint64(num_shards, 1, "number of shards in the system");

/**
 * Experiment settings.
 */
DEFINE_uint64(exp_duration, 30, "duration (in seconds) of experiment");
DEFINE_uint64(warmup_secs, 5, "time (in seconds) to warm up system before"
    " recording stats");
DEFINE_uint64(tput_interval, 0, "time (in seconds) between throughput"
    " measurements");
DEFINE_uint64(num_clients, 1, "number of clients to run in this process");
DEFINE_uint64(num_requests, 100, "number of requests (transactions) per"
    " client");
DEFINE_int32(closest_replica, -1, "index of the replica closest to the client");
DEFINE_uint64(delay, 0, "simulated communication delay");
DEFINE_int32(clock_skew, 0, "difference between real clock and TrueTime");
DEFINE_int32(clock_error, 0, "maximum error for clock");

const std::string protocol_args[] = {
	"txn-l",
  "txn-s",
  "qw",
  "occ",
  "lock",
  "span-occ",
  "span-lock"
};
const protomode_t protomodes[] {
  PROTO_TAPIR,
  PROTO_TAPIR,
  PROTO_WEAK,
  PROTO_STRONG,
  PROTO_STRONG,
  PROTO_STRONG,
  PROTO_STRONG
};
const strongstore::Mode strongmodes[] {
  strongstore::Mode::MODE_UNKNOWN,
  strongstore::Mode::MODE_UNKNOWN,
  strongstore::Mode::MODE_UNKNOWN,
  strongstore::Mode::MODE_OCC,
  strongstore::Mode::MODE_LOCK,
  strongstore::Mode::MODE_SPAN_OCC,
  strongstore::Mode::MODE_SPAN_LOCK
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
  "tpcc"
};
const benchmode_t benchmodes[] {
  BENCH_RETWIS,
  BENCH_TPCC
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
 * Retwis settings
 */
DEFINE_string(keys_path, "", "path to file containing keys in the system"
		" (for retwis)");
DEFINE_uint64(num_keys, 0, "number of keys to generate (for retwis");

/**
 * TPCC settings
 */
DEFINE_int32(warehouse_per_shard, 1, "number of warehouses per shard"
		" (for tpcc)");
DEFINE_int32(clients_per_warehouse, 1, "number of clients per warehouse"
		" (for tpcc)");
DEFINE_int32(remote_item_milli_p, 0, "remote item milli p (for tpcc)");

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

  // parse retwis settings
  std::vector<std::string> keys;
  if (benchMode == BENCH_RETWIS) {
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

  // parse tpcc settings
	int total_warehouses = FLAGS_num_shards * FLAGS_warehouse_per_shard;
  
  UDPTransport transport(0.0, 0.0, 0, false);

  std::vector<::Client *> clients;
  std::vector<::BenchmarkClient *> benchClients;
  KeySelector *keySelector = new UniformKeySelector(keys);

  for (size_t i = 0; i < FLAGS_num_clients; i++) {
    Client *client;
    switch (mode) {
      case PROTO_TAPIR: {
        client = new tapirstore::Client(FLAGS_config_path, FLAGS_num_shards,
            FLAGS_closest_replica, &transport, TrueTime(FLAGS_clock_skew,
              FLAGS_clock_error));
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
      default:
        NOT_REACHABLE();
    }

	  BenchmarkClient *bench;
	  switch (benchMode) {
      case BENCH_RETWIS:
        bench = new retwis::RetwisClient(keySelector, *client, transport,
            FLAGS_num_requests, FLAGS_delay, FLAGS_warmup_secs,
            FLAGS_tput_interval);
        break;
      case BENCH_TPCC:
        bench = new tpcc::TPCCClient(*client, transport, FLAGS_num_requests,
            FLAGS_delay, FLAGS_warmup_secs, FLAGS_tput_interval);
        break;
      default:
        NOT_REACHABLE();
    }

	  transport.Timer(0, [=]() { bench->Start(); });
    clients.push_back(client);
    benchClients.push_back(bench);
  }

	std::string latencyFile;
  std::string latencyRawFile;
  std::vector<uint64_t> latencies;
  Timeout checkTimeout(&transport, 100, [&]() {
    for (auto x : benchClients) {
      if (!x->cooldownDone) {
        return;
      }
    }
    Notice("All clients done.");

    Latency_t sum;
    _Latency_Init(&sum, "total");
    for (unsigned int i = 0; i < benchClients.size(); i++) {
      Latency_Sum(&sum, &benchClients[i]->latency);
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
    exit(0);
  });
  checkTimeout.Start();

  transport.Run();
	return 0;
}

