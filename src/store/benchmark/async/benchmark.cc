// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * store/benchmark/tpccClient.cc:
 *   Benchmarking client for tpcc.
 *
 **********************************************************************/

#include "lib/keymanager.h"
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
#include "store/benchmark/async/tpcc/sync/tpcc_client.h"
#include "store/benchmark/async/tpcc/async/tpcc_client.h"
#include "store/mortystore/client.h"
#include "store/benchmark/async/smallbank/smallbank_client.h"
#include "store/janusstore/client.h"
#include "store/indicusstore/client.h"
#include "store/pbftstore/client.h"
#include "store/common/frontend/one_shot_client.h"
#include "store/common/frontend/async_one_shot_adapter_client.h"
#include "store/benchmark/async/common/zipf_key_selector.h"

#include <gflags/gflags.h>

#include <algorithm>
#include <atomic>
#include <sstream>
#include <thread>
#include <vector>

enum protomode_t {
	PROTO_UNKNOWN,
	PROTO_TAPIR,
	PROTO_WEAK,
	PROTO_STRONG,
  PROTO_JANUS,
  PROTO_MORTY,
  PROTO_INDICUS,
	PROTO_PBFT,
};

enum benchmode_t {
  BENCH_UNKNOWN,
  BENCH_RETWIS,
  BENCH_TPCC,
  BENCH_SMALLBANK_SYNC,
  BENCH_RW,
  BENCH_TPCC_SYNC
};

enum keysmode_t {
  KEYS_UNKNOWN,
  KEYS_UNIFORM,
  KEYS_ZIPF
};

enum transmode_t {
	TRANS_UNKNOWN,
  TRANS_UDP,
  TRANS_TCP,
};

enum read_quorum_t {
  READ_QUORUM_UNKNOWN,
  READ_QUORUM_ONE,
  READ_QUORUM_ONE_HONEST,
  READ_QUORUM_MAJORITY_HONEST,
  READ_QUORUM_MAJORITY,
  READ_QUORUM_ALL
};

enum read_dep_t {
  READ_DEP_UNKNOWN,
  READ_DEP_ONE,
  READ_DEP_ONE_HONEST
};

enum read_messages_t {
  READ_MESSAGES_UNKNOWN,
  READ_MESSAGES_READ_QUORUM,
  READ_MESSAGES_MAJORITY,
  READ_MESSAGES_ALL
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

const std::string read_quorum_args[] = {
	"one",
  "one-honest",
  "majority-honest",
  "majority",
  "all"
};
const read_quorum_t read_quorums[] {
	READ_QUORUM_ONE,
  READ_QUORUM_ONE_HONEST,
  READ_QUORUM_MAJORITY_HONEST,
  READ_QUORUM_MAJORITY,
  READ_QUORUM_ALL
};
static bool ValidateReadQuorum(const char* flagname,
    const std::string &value) {
  int n = sizeof(read_quorum_args);
  for (int i = 0; i < n; ++i) {
    if (value == read_quorum_args[i]) {
      return true;
    }
  }
  std::cerr << "Invalid value for --" << flagname << ": " << value << std::endl;
  return false;
}
DEFINE_string(indicus_read_quorum, read_quorum_args[0], "size of read quorums"
    " (for Indicus)");
DEFINE_validator(indicus_read_quorum, &ValidateReadQuorum);
const std::string read_dep_args[] = {
  "one-honest",
	"one"
};
const read_dep_t read_deps[] {
  READ_DEP_ONE_HONEST,
  READ_DEP_ONE
};
static bool ValidateReadDep(const char* flagname,
    const std::string &value) {
  int n = sizeof(read_dep_args);
  for (int i = 0; i < n; ++i) {
    if (value == read_dep_args[i]) {
      return true;
    }
  }
  std::cerr << "Invalid value for --" << flagname << ": " << value << std::endl;
  return false;
}
DEFINE_string(indicus_read_dep, read_dep_args[0], "number of identical prepared"
    " to claim dependency (for Indicus)");
DEFINE_validator(indicus_read_dep, &ValidateReadDep);
const std::string read_messages_args[] = {
	"read-quorum",
  "majority",
  "all"
};
const read_messages_t read_messagess[] {
  READ_MESSAGES_READ_QUORUM,
  READ_MESSAGES_MAJORITY,
  READ_MESSAGES_ALL
};
static bool ValidateReadMessages(const char* flagname,
    const std::string &value) {
  int n = sizeof(read_messages_args);
  for (int i = 0; i < n; ++i) {
    if (value == read_messages_args[i]) {
      return true;
    }
  }
  std::cerr << "Invalid value for --" << flagname << ": " << value << std::endl;
  return false;
}
DEFINE_string(indicus_read_messages, read_messages_args[0], "number of replicas"
    " to send messages for reads (for Indicus)");
DEFINE_validator(indicus_read_messages, &ValidateReadMessages);
DEFINE_bool(indicus_sign_messages, false, "add signatures to messages as"
    " necessary to prevent impersonation (for Indicus)");
DEFINE_bool(indicus_validate_proofs, false, "send and validate proofs as"
    " necessary to check Byzantine behavior (for Indicus)");
DEFINE_string(indicus_key_path, "", "path to directory containing public and"
    " private keys (for Indicus)");

DEFINE_bool(debug_stats, false, "record stats related to debugging");

const std::string trans_args[] = {
  "udp",
	"tcp"
};

const transmode_t transmodes[] {
  TRANS_UDP,
	TRANS_TCP
};
static bool ValidateTransMode(const char* flagname,
    const std::string &value) {
  int n = sizeof(trans_args);
  for (int i = 0; i < n; ++i) {
    if (value == trans_args[i]) {
      return true;
    }
  }
  std::cerr << "Invalid value for --" << flagname << ": " << value << std::endl;
  return false;
}
DEFINE_string(trans_protocol, trans_args[0], "transport protocol to use for"
		" passing messages");
DEFINE_validator(trans_protocol, &ValidateTransMode);

const std::string protocol_args[] = {
	"txn-l",
  "txn-s",
  "qw",
  "occ",
  "lock",
  "span-occ",
  "span-lock",
  "janus",
  "morty",
  "indicus",
	"pbft"
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
  PROTO_MORTY,
  PROTO_INDICUS,
	PROTO_PBFT
};
const strongstore::Mode strongmodes[] {
  strongstore::Mode::MODE_UNKNOWN,
  strongstore::Mode::MODE_UNKNOWN,
  strongstore::Mode::MODE_UNKNOWN,
  strongstore::Mode::MODE_OCC,
  strongstore::Mode::MODE_LOCK,
  strongstore::Mode::MODE_SPAN_OCC,
  strongstore::Mode::MODE_SPAN_LOCK,
  strongstore::Mode::MODE_UNKNOWN,
  strongstore::Mode::MODE_UNKNOWN,
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
  "rw",
  "tpcc-sync"
};
const benchmode_t benchmodes[] {
  BENCH_RETWIS,
  BENCH_TPCC,
  BENCH_SMALLBANK_SYNC,
  BENCH_RW,
  BENCH_TPCC_SYNC
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
DEFINE_string(closest_replicas, "", "space-separated list of replica indices in"
    " order of proximity to client(s)");
DEFINE_uint64(delay, 0, "simulated communication delay");
DEFINE_int32(clock_skew, 0, "difference between real clock and TrueTime");
DEFINE_int32(clock_error, 0, "maximum error for clock");
DEFINE_string(stats_file, "", "path to output stats file.");
DEFINE_int32(abort_backoff, 100, "sleep exponentially increasing amount after abort.");
DEFINE_bool(retry_aborted, true, "retry aborted transactions.");
DEFINE_int32(max_attempts, -1, "max number of attempts per transaction (or -1"
    " for unlimited).");
DEFINE_int32(message_timeout, 10000, "length of timeout for messages in ms.");

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
DEFINE_uint64(num_ops_txn, 1, "number of ops in each txn"
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
DEFINE_double(hotspot_probability, 0.9, "probability of ending in hotspot");
DEFINE_int32(timeout, 5000, "timeout in ms (for smallbank)");
DEFINE_string(customer_name_file_path, "smallbank_names", "path to file"
    " containing names to be loaded (for smallbank)");

DEFINE_LATENCY(op);

void FlushStats(const std::vector<::BenchmarkClient *> &benchClients,
  const std::vector<::AsyncClient *> &asyncClients) {
  if (FLAGS_stats_file.size() > 0) {
    Stats total;
    for (unsigned int i = 0; i < benchClients.size(); i++) {
      total.Merge(benchClients[i]->GetStats());
    }
    for (unsigned int i = 0; i < asyncClients.size(); i++) {
      total.Merge(asyncClients[i]->GetStats());
    }
    total.ExportJSON(FLAGS_stats_file);
  }
}

int main(int argc, char **argv) {
  gflags::SetUsageMessage(
           "executes transactions from various transactional workload\n"
"           benchmarks against various distributed replicated transaction\n"
"           processing systems.");
	gflags::ParseCommandLineFlags(&argc, &argv, true);

  // parse transport protocol
  transmode_t trans = TRANS_UNKNOWN;
  int numTransModes = sizeof(trans_args);
  for (int i = 0; i < numTransModes; ++i) {
    if (FLAGS_trans_protocol == trans_args[i]) {
      trans = transmodes[i];
      break;
    }
  }
  if (trans == TRANS_UNKNOWN) {
    std::cerr << "Unknown transport protocol." << std::endl;
    return 1;
  }

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

  // parse read quorum
  read_quorum_t read_quorum = READ_QUORUM_UNKNOWN;
  int numReadQuorums = sizeof(read_quorum_args);
  for (int i = 0; i < numReadQuorums; ++i) {
    if (FLAGS_indicus_read_quorum == read_quorum_args[i]) {
      read_quorum = read_quorums[i];
      break;
    }
  }
  if (mode == PROTO_INDICUS && read_quorum == READ_QUORUM_UNKNOWN) {
    std::cerr << "Unknown read quorum." << std::endl;
    return 1;
  }

  // parse read messages
  read_messages_t read_messages = READ_MESSAGES_UNKNOWN;
  int numReadMessagess = sizeof(read_messages_args);
  for (int i = 0; i < numReadMessagess; ++i) {
    if (FLAGS_indicus_read_messages == read_messages_args[i]) {
      read_messages = read_messagess[i];
      break;
    }
  }
  if (mode == PROTO_INDICUS && read_messages == READ_MESSAGES_UNKNOWN) {
    std::cerr << "Unknown read messages." << std::endl;
    return 1;
  }

  // parse read dep
  read_dep_t read_dep = READ_DEP_UNKNOWN;
  int numReadDeps = sizeof(read_dep_args);
  for (int i = 0; i < numReadDeps; ++i) {
    if (FLAGS_indicus_read_dep == read_dep_args[i]) {
      read_dep = read_deps[i];
      break;
    }
  }
  if (mode == PROTO_INDICUS && read_dep == READ_DEP_UNKNOWN) {
    std::cerr << "Unknown read dep." << std::endl;
    return 1;
  }

  // parse closest replicas
  std::vector<int> closestReplicas;
  std::stringstream iss(FLAGS_closest_replicas);
  int replica;
  iss >> replica;
  while (!iss.fail()) {
    closestReplicas.push_back(replica);
    iss >> replica;
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

  Transport *transport;
  switch (trans) {
    case TRANS_TCP:
      transport = new TCPTransport(0.0, 0.0, 0, false);
      break;
    case TRANS_UDP:
      transport = new UDPTransport(0.0, 0.0, 0, nullptr);
      break;
    default:
      NOT_REACHABLE();
  }

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
    case BENCH_TPCC_SYNC:
      part = warehouse_district_partitioner(FLAGS_tpcc_num_warehouses);
      break;
    default:
      part = default_partitioner;
      break;
  }

	std::string latencyFile;
  std::string latencyRawFile;
  std::vector<uint64_t> latencies;
  std::atomic<size_t> clientsDone(0UL);

  bench_done_callback bdcb = [&]() {
    ++clientsDone;
    if (clientsDone == FLAGS_num_clients) {
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

      FlushStats(benchClients, asyncClients);
      transport->Stop();
    }
  };

  std::ifstream configStream(FLAGS_config_path);
  if (configStream.fail()) {
    std::cerr << "Unable to read configuration file: " << FLAGS_config_path
              << std::endl;
    return -1;
  }
  transport::Configuration *config = new transport::Configuration(configStream);
  KeyManager *keyManager = new KeyManager(FLAGS_indicus_key_path);

  if (closestReplicas.size() > 0 && closestReplicas.size() != static_cast<size_t>(config->n)) {
    std::cerr << "If specifying closest replicas, must specify all "
               << config->n << "; only specified "
               << closestReplicas.size() << std::endl;
    return 1;
  }

  for (size_t i = 0; i < FLAGS_num_clients; i++) {
    Client *client = nullptr;
    AsyncClient *asyncClient = nullptr;
    SyncClient *syncClient = nullptr;
    OneShotClient *oneShotClient = nullptr;

    switch (mode) {
      case PROTO_TAPIR: {
        client = new tapirstore::Client(config, (FLAGS_client_id << 3) | i,
            FLAGS_num_shards, FLAGS_num_groups, FLAGS_closest_replica,
            transport, part, FLAGS_tapir_sync_commit, TrueTime(FLAGS_clock_skew,
              FLAGS_clock_error));
        break;
      }
      case PROTO_JANUS: {
        oneShotClient = new janusstore::Client(config,
            FLAGS_num_shards, FLAGS_closest_replica, transport);
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
        asyncClient = new mortystore::Client(config,
            (FLAGS_client_id << 3) | i, FLAGS_num_shards, FLAGS_num_groups,
            FLAGS_closest_replica, transport, part, FLAGS_debug_stats);
        break;
      }
      case PROTO_INDICUS: {
        uint64_t readQuorumSize = 0;
        switch (read_quorum) {
          case READ_QUORUM_ONE:
            readQuorumSize = 1;
            break;
          case READ_QUORUM_ONE_HONEST:
            readQuorumSize = config->f + 1;
            break;
          case READ_QUORUM_MAJORITY_HONEST:
            readQuorumSize = config->f * 2 + 1;
            break;
          case READ_QUORUM_MAJORITY:
            readQuorumSize = (config->n + 1) / 2;
            break;
          case READ_QUORUM_ALL:
            readQuorumSize = config->f * 4 + 1;
            break;
          default:
            NOT_REACHABLE();
        }

        uint64_t readMessages = 0;
        switch (read_messages) {
          case READ_MESSAGES_READ_QUORUM:
            readMessages = readQuorumSize;
            break;
          case READ_MESSAGES_MAJORITY:
            readQuorumSize = (config->n + 1) / 2;
            break;
          case READ_MESSAGES_ALL:
            readQuorumSize = config->n;
            break;
          default:
            NOT_REACHABLE();
        }
        UW_ASSERT(readMessages >= readQuorumSize);
        
        uint64_t readDepSize = 0;
        switch (read_dep) {
          case READ_DEP_ONE:
            readDepSize = 1;
            break;
          case READ_DEP_ONE_HONEST:
            readDepSize = config->f + 1;
            break;
          default:
            NOT_REACHABLE();
        }

        client = new indicusstore::Client(config, (FLAGS_client_id << 3),
            FLAGS_num_shards,
            FLAGS_num_groups, closestReplicas, transport, part,
            FLAGS_tapir_sync_commit, readMessages, readQuorumSize, readDepSize,
            FLAGS_indicus_sign_messages, FLAGS_indicus_validate_proofs,
            keyManager, TrueTime(FLAGS_clock_skew, FLAGS_clock_error));
        break;
      }
			case PROTO_PBFT: {
				uint64_t readQuorumSize = 0;
        switch (read_quorum) {
          case READ_QUORUM_ONE:
            readQuorumSize = 1;
            break;
          case READ_QUORUM_ONE_HONEST:
            readQuorumSize = config->f + 1;
            break;
          case READ_QUORUM_MAJORITY_HONEST:
            readQuorumSize = config->f * 2 + 1;
            break;
          default:
            NOT_REACHABLE();
        }

        client = new pbftstore::Client(*config, FLAGS_num_shards,
            FLAGS_num_groups, transport, part,
            readQuorumSize,
            FLAGS_indicus_sign_messages, FLAGS_indicus_validate_proofs,
            keyManager, TrueTime(FLAGS_clock_skew, FLAGS_clock_error));
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
          asyncClient = new AsyncAdapterClient(client, FLAGS_message_timeout);
        }
        break;
      case BENCH_SMALLBANK_SYNC:
      case BENCH_TPCC_SYNC:
        if (syncClient == nullptr) {
          UW_ASSERT(client != nullptr);
          syncClient = new SyncClient(client);
        }
        break;
      default:
        NOT_REACHABLE();
    }

    uint32_t seed = (FLAGS_client_id << 4) | i;
	  BenchmarkClient *bench;
	  switch (benchMode) {
      case BENCH_RETWIS:
        UW_ASSERT(asyncClient != nullptr);
        bench = new retwis::RetwisClient(keySelector, *asyncClient, *transport,
            seed,
            FLAGS_num_requests, FLAGS_exp_duration, FLAGS_delay,
            FLAGS_warmup_secs, FLAGS_cooldown_secs, FLAGS_tput_interval,
            FLAGS_abort_backoff, FLAGS_retry_aborted, FLAGS_max_attempts);
        break;
      case BENCH_TPCC:
        UW_ASSERT(asyncClient != nullptr);
        bench = new tpcc::AsyncTPCCClient(*asyncClient, *transport,
            seed,
            FLAGS_num_requests, FLAGS_exp_duration, FLAGS_delay,
            FLAGS_warmup_secs, FLAGS_cooldown_secs, FLAGS_tput_interval,
            FLAGS_tpcc_num_warehouses, FLAGS_tpcc_w_id, FLAGS_tpcc_C_c_id,
            FLAGS_tpcc_C_c_last, FLAGS_tpcc_new_order_ratio,
            FLAGS_tpcc_delivery_ratio, FLAGS_tpcc_payment_ratio,
            FLAGS_tpcc_order_status_ratio, FLAGS_tpcc_stock_level_ratio,
            FLAGS_static_w_id, FLAGS_abort_backoff,
            FLAGS_retry_aborted, FLAGS_max_attempts);
        break;
      case BENCH_TPCC_SYNC:
        UW_ASSERT(syncClient != nullptr);
        bench = new tpcc::SyncTPCCClient(*syncClient, *transport,
            seed,
            FLAGS_num_requests, FLAGS_exp_duration, FLAGS_delay,
            FLAGS_warmup_secs, FLAGS_cooldown_secs, FLAGS_tput_interval,
            FLAGS_tpcc_num_warehouses, FLAGS_tpcc_w_id, FLAGS_tpcc_C_c_id,
            FLAGS_tpcc_C_c_last, FLAGS_tpcc_new_order_ratio,
            FLAGS_tpcc_delivery_ratio, FLAGS_tpcc_payment_ratio,
            FLAGS_tpcc_order_status_ratio, FLAGS_tpcc_stock_level_ratio,
            FLAGS_static_w_id, FLAGS_abort_backoff,
            FLAGS_retry_aborted, FLAGS_max_attempts, FLAGS_message_timeout);
        break;
      case BENCH_SMALLBANK_SYNC:
        UW_ASSERT(syncClient != nullptr);
        bench = new smallbank::SmallbankClient(*syncClient, *transport,
            seed,
            FLAGS_num_requests, FLAGS_exp_duration, FLAGS_delay,
            FLAGS_warmup_secs, FLAGS_cooldown_secs, FLAGS_tput_interval,
            FLAGS_abort_backoff, FLAGS_retry_aborted, FLAGS_max_attempts,
            FLAGS_timeout, FLAGS_balance_ratio, FLAGS_deposit_checking_ratio,
            FLAGS_transact_saving_ratio, FLAGS_amalgamate_ratio,
            FLAGS_num_hotspots, FLAGS_num_customers - FLAGS_num_hotspots, FLAGS_hotspot_probability,
            FLAGS_customer_name_file_path);
        break;
      case BENCH_RW:
        UW_ASSERT(asyncClient != nullptr);
        bench = new rw::RWClient(keySelector, FLAGS_num_ops_txn,
            *asyncClient, *transport, seed,
            FLAGS_num_requests, FLAGS_exp_duration, FLAGS_delay,
            FLAGS_warmup_secs, FLAGS_cooldown_secs, FLAGS_tput_interval,
            FLAGS_abort_backoff, FLAGS_retry_aborted,
            FLAGS_max_attempts);
        break;
      default:
        NOT_REACHABLE();
    }

    switch (benchMode) {
      case BENCH_RETWIS:
      case BENCH_TPCC:
      case BENCH_RW:
        // async benchmarks
	      transport->Timer(0, [bench, bdcb]() { bench->Start(bdcb); });
        break;
      case BENCH_SMALLBANK_SYNC:
      case BENCH_TPCC_SYNC: {
        SyncTransactionBenchClient *syncBench = dynamic_cast<SyncTransactionBenchClient *>(bench);
        UW_ASSERT(syncBench != nullptr);
        threads.push_back(new std::thread([syncBench, bdcb](){
            syncBench->Start([](){});
            while (!syncBench->IsFullyDone()) {
              syncBench->StartLatency();
              transaction_status_t result;
              syncBench->SendNext(&result);
              syncBench->IncrementSent(result);
            }
            bdcb();
        }));
        break;
      }
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

  if (threads.size() > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
  }

  transport->Run();

  delete config;
  delete keyManager;

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
  delete transport;
	return 0;
}
