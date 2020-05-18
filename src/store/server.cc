// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * store/tapirstore/server.cc:
 *   Implementation of a single transactional key-value server.
 *
 * Copyright 2015 Irene Zhang <iyzhang@cs.washington.edu>
 *                Naveen Kr. Sharma <naveenks@cs.washington.edu>
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

#include <csignal>

#include <valgrind/callgrind.h>

#include "lib/keymanager.h"
#include "lib/transport.h"
#include "lib/tcptransport.h"
#include "lib/udptransport.h"
#include "lib/io_utils.h"

#include "store/common/partitioner.h"
#include "store/server.h"
#include "store/strongstore/server.h"
#include "store/tapirstore/server.h"
#include "store/weakstore/server.h"
#include "store/janusstore/server.h"
#include "store/mortystore/server.h"
#include "store/indicusstore/server.h"
#include "store/pbftstore/replica.h"
#include "store/pbftstore/server.h"

#include "store/benchmark/async/tpcc/tpcc-proto.pb.h"
#include "store/indicusstore/common.h"

#include <gflags/gflags.h>

enum protocol_t {
	PROTO_UNKNOWN,
	PROTO_TAPIR,
	PROTO_WEAK,
	PROTO_STRONG,
  PROTO_JANUS,
  PROTO_MORTY,
  PROTO_INDICUS,
	PROTO_PBFT
};

enum transmode_t {
	TRANS_UNKNOWN,
  TRANS_UDP,
  TRANS_TCP,
};

enum occ_type_t {
  OCC_TYPE_UNKNOWN,
  OCC_TYPE_MVTSO,
  OCC_TYPE_TAPIR
};

enum read_dep_t {
  READ_DEP_UNKNOWN,
  READ_DEP_ONE,
  READ_DEP_ONE_HONEST
};

/**
 * System settings.
 */
DEFINE_string(config_path, "", "path to shard configuration file");
DEFINE_uint64(replica_idx, 0, "index of replica in shard configuration file");
DEFINE_uint64(group_idx, 0, "index of the group to which this replica belongs");
DEFINE_uint64(num_groups, 1, "number of replica groups in the system");
DEFINE_uint64(num_shards, 1, "number of shards in the system");
DEFINE_bool(debug_stats, false, "record stats related to debugging");

const std::string protocol_args[] = {
	"tapir",
  "weak",
  "strong",
  "janus",
  "morty",
  "indicus",
	"pbft"
};
const protocol_t protos[] {
  PROTO_TAPIR,
  PROTO_WEAK,
  PROTO_STRONG,
  PROTO_JANUS,
  PROTO_MORTY,
  PROTO_INDICUS,
	PROTO_PBFT
};
static bool ValidateProtocol(const char* flagname,
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
DEFINE_string(protocol, protocol_args[0],	"the protocol to use during this"
    " experiment");
DEFINE_validator(protocol, &ValidateProtocol);

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

const std::string partitioner_args[] = {
	"default",
  "warehouse_dist_items",
  "warehouse"
};
const partitioner_t parts[] {
  DEFAULT,
  WAREHOUSE_DIST_ITEMS,
  WAREHOUSE
};
static bool ValidatePartitioner(const char* flagname,
    const std::string &value) {
  int n = sizeof(partitioner_args);
  for (int i = 0; i < n; ++i) {
    if (value == partitioner_args[i]) {
      return true;
    }
  }
  std::cerr << "Invalid value for --" << flagname << ": " << value << std::endl;
  return false;
}
DEFINE_string(partitioner, partitioner_args[0],	"the partitioner to use during this"
    " experiment");
DEFINE_validator(partitioner, &ValidatePartitioner);

/**
 * TPCC settings.
 */
DEFINE_int32(tpcc_num_warehouses, 1, "number of warehouses (for tpcc)");

/**
 * TAPIR settings.
 */
DEFINE_bool(tapir_linearizable, true, "run TAPIR in linearizable mode");

/**
 * StrongStore settings.
 */
const std::string strongmode_args[] = {
	"lock",
  "occ",
  "span-lock",
  "span-occ"
};
const strongstore::Mode strongmodes[] {
  strongstore::MODE_LOCK,
  strongstore::MODE_OCC,
  strongstore::MODE_SPAN_LOCK,
  strongstore::MODE_SPAN_OCC
};
static bool ValidateStrongMode(const char* flagname,
    const std::string &value) {
  int n = sizeof(strongmode_args);
  for (int i = 0; i < n; ++i) {
    if (value == strongmode_args[i]) {
      return true;
    }
  }
  std::cerr << "Invalid value for --" << flagname << ": " << value << std::endl;
  return false;
}
DEFINE_string(strongmode, strongmode_args[0],	"the protocol to use during this"
    " experiment");
DEFINE_validator(strongmode, &ValidateStrongMode);

/**
 * Morty settings.
 */
DEFINE_uint64(prepare_batch_period, 0, "length of batches for deterministic prepare message"
    " processing.");

/**
 * Indicus settings.
 */
DEFINE_uint64(indicus_time_delta, 2000, "max clock skew allowed for concurrency"
    " control (for Indicus)");
DEFINE_bool(indicus_shared_mem_batch, false, "use shared memory batches for"
    " signing messages (for Indicus)");
DEFINE_bool(indicus_sign_messages, false, "add signatures to messages as"
    " necessary to prevent impersonation (for Indicus)");
DEFINE_bool(indicus_validate_proofs, false, "send and validate proofs as"
    " necessary to check Byzantine behavior (for Indicus)");
DEFINE_bool(indicus_hash_digest, false, "use hash function compute transaction"
    " digest (for Indicus)");
DEFINE_bool(indicus_verify_deps, true, "check signatures of transaction"
    " depdendencies (for Indicus)");
DEFINE_bool(indicus_read_reply_batch, false, "wait to reply to reads until batch"
    " is ready (for Indicus)");
DEFINE_bool(indicus_adjust_batch_size, false, "dynamically adjust batch size"
    " every sig_batch_timeout (for Indicus)");
DEFINE_uint64(indicus_sig_batch, 1, "signature batch size"
    " sig batch size (for Indicus)");
DEFINE_uint64(indicus_sig_batch_timeout, 10, "signature batch timeout ms"
    " sig batch timeout (for Indicus)");
DEFINE_string(indicus_key_path, "", "path to directory containing public and"
    " private keys (for Indicus)");
DEFINE_int64(indicus_max_dep_depth, -1, "maximum length of dependency chain"
    " allowed by honest replicas [-1 is no maximum, -2 is no deps] (for Indicus)");
DEFINE_uint64(indicus_key_type, 2, "key type (see create keys for mappings)"
    " key type (for Indicus)");
const std::string occ_type_args[] = {
	"tapir",
  "mvtso"
};
const occ_type_t occ_types[] {
  OCC_TYPE_MVTSO,
	OCC_TYPE_TAPIR
};
static bool ValidateOCCType(const char* flagname,
    const std::string &value) {
  int n = sizeof(occ_type_args);
  for (int i = 0; i < n; ++i) {
    if (value == occ_type_args[i]) {
      return true;
    }
  }
  std::cerr << "Invalid value for --" << flagname << ": " << value << std::endl;
  return false;
}
DEFINE_string(indicus_occ_type, occ_type_args[0], "Type of OCC for validating"
    " transactions (for Indicus)");
DEFINE_validator(indicus_occ_type, &ValidateOCCType);
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

/**
 * Experiment settings.
 */
DEFINE_int32(clock_skew, 0, "difference between real clock and TrueTime");
DEFINE_int32(clock_error, 0, "maximum error for clock");
DEFINE_string(stats_file, "", "path to file for server stats");

/**
 * Benchmark settings.
 */
DEFINE_string(keys_path, "", "path to file containing keys in the system");
DEFINE_uint64(num_keys, 0, "number of keys to generate");
DEFINE_string(data_file_path, "", "path to file containing key-value pairs to be loaded");

Server *server = nullptr;
TransportReceiver *replica = nullptr;
::Transport *tport = nullptr;
Partitioner *part = nullptr;

void Cleanup(int signal);

int main(int argc, char **argv) {
  gflags::SetUsageMessage(
           "runs a replica for a distributed replicated transaction\n"
"           processing system.");
	gflags::ParseCommandLineFlags(&argc, &argv, true);

  Notice("Starting server.");

  // parse configuration
  std::ifstream configStream(FLAGS_config_path);
  if (configStream.fail()) {
    std::cerr << "Unable to read configuration file: " << FLAGS_config_path
              << std::endl;
  }

  // parse protocol and mode
  protocol_t proto = PROTO_UNKNOWN;
  int numProtos = sizeof(protocol_args);
  for (int i = 0; i < numProtos; ++i) {
    if (FLAGS_protocol == protocol_args[i]) {
      proto = protos[i];
      break;
    }
  }

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

  transport::Configuration config(configStream);

  if (FLAGS_replica_idx >= static_cast<uint64_t>(config.n)) {
    std::cerr << "Replica index " << FLAGS_replica_idx << " is out of bounds"
                 "; only " << config.n << " replicas defined" << std::endl;
  }

  if (proto == PROTO_UNKNOWN) {
    std::cerr << "Unknown protocol." << std::endl;
    return 1;
  }

  strongstore::Mode strongMode = strongstore::Mode::MODE_UNKNOWN;
  if (proto == PROTO_STRONG) {
    int numStrongModes = sizeof(strongmode_args);
    for (int i = 0; i < numStrongModes; ++i) {
      if (FLAGS_strongmode == strongmode_args[i]) {
        strongMode = strongmodes[i];
        break;
      }
    }
  }

  switch (trans) {
    case TRANS_TCP:
      tport = new TCPTransport(0.0, 0.0, 0, false);
      break;
    case TRANS_UDP:
      tport = new UDPTransport(0.0, 0.0, 0, nullptr);
      break;
    default:
      NOT_REACHABLE();
  }

  // parse protocol and mode
  partitioner_t partType = DEFAULT;
  int numParts = sizeof(partitioner_args);
  for (int i = 0; i < numParts; ++i) {
    if (FLAGS_partitioner == partitioner_args[i]) {
      partType = parts[i];
      break;
    }
  }

  std::mt19937 unused;
  switch (partType) {
    case DEFAULT:
      part = new DefaultPartitioner();
      break;
    case WAREHOUSE_DIST_ITEMS:
      part = new WarehouseDistItemsPartitioner(FLAGS_tpcc_num_warehouses);
      break;
    case WAREHOUSE:
      part = new WarehousePartitioner(FLAGS_tpcc_num_warehouses, unused);
      break;
    default:
      NOT_REACHABLE();
  }

  // parse occ type
  occ_type_t occ_type = OCC_TYPE_UNKNOWN;
  int numOCCTypes = sizeof(occ_type_args);
  for (int i = 0; i < numOCCTypes; ++i) {
    if (FLAGS_indicus_occ_type == occ_type_args[i]) {
      occ_type = occ_types[i];
      break;
    }
  }
  if (proto == PROTO_INDICUS && occ_type == OCC_TYPE_UNKNOWN) {
    std::cerr << "Unknown occ type." << std::endl;
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
  if (proto == PROTO_INDICUS && read_dep == READ_DEP_UNKNOWN) {
    std::cerr << "Unknown read dep." << std::endl;
    return 1;
  }

	crypto::KeyType keyType;
  switch (FLAGS_indicus_key_type) {
  case 0:
    keyType = crypto::RSA;
    break;
  case 1:
    keyType = crypto::ECDSA;
    break;
  case 2:
    keyType = crypto::ED25;
    break;
  case 3:
    keyType = crypto::SECP;
    break;
  default:
    throw "unimplemented";
  }
  KeyManager keyManager(FLAGS_indicus_key_path, keyType, true);

  switch (proto) {
    case PROTO_TAPIR: {
      server = new tapirstore::Server(FLAGS_tapir_linearizable);
      replica = new replication::ir::IRReplica(config, FLAGS_group_idx, FLAGS_replica_idx,
          tport, dynamic_cast<replication::ir::IRAppReplica *>(server));
      break;
    }
    case PROTO_WEAK: {
      server = new weakstore::Server(config, FLAGS_group_idx, FLAGS_replica_idx, tport);
      break;
    }
    case PROTO_STRONG: {
      server = new strongstore::Server(strongMode, FLAGS_clock_skew,
          FLAGS_clock_error);
      replica = new replication::vr::VRReplica(config, FLAGS_group_idx, FLAGS_replica_idx,
          tport, 1, dynamic_cast<replication::AppReplica *>(server));
      break;
    }
    case PROTO_JANUS: {
      server = new janusstore::Server(config, FLAGS_group_idx, FLAGS_replica_idx, tport);
      break;
    }
    case PROTO_MORTY: {
      server = new mortystore::Server(config, FLAGS_group_idx, FLAGS_replica_idx,
          tport, FLAGS_debug_stats, FLAGS_prepare_batch_period);
      break;
    }
    case PROTO_INDICUS: {
      uint64_t readDepSize = 0;
      switch (read_dep) {
        case READ_DEP_ONE:
          readDepSize = 1;
          break;
        case READ_DEP_ONE_HONEST:
          readDepSize = config.f + 1;
          break;
        default:
          NOT_REACHABLE();
      }
      indicusstore::OCCType indicusOCCType;
      switch (occ_type) {
        case OCC_TYPE_TAPIR:
          indicusOCCType = indicusstore::TAPIR;
          break;
        case OCC_TYPE_MVTSO:
          indicusOCCType = indicusstore::MVTSO;
          break;
        default:
          NOT_REACHABLE();
      }
      uint64_t timeDelta = (FLAGS_indicus_time_delta / 1000) << 32;
      timeDelta = timeDelta | (FLAGS_indicus_time_delta % 1000) * 1000;
			indicusstore::Parameters params(FLAGS_indicus_sign_messages,
				FLAGS_indicus_validate_proofs, FLAGS_indicus_hash_digest,
				FLAGS_indicus_verify_deps, FLAGS_indicus_sig_batch,
        FLAGS_indicus_max_dep_depth, readDepSize,
        FLAGS_indicus_read_reply_batch, FLAGS_indicus_adjust_batch_size,
        FLAGS_indicus_shared_mem_batch);
      server = new indicusstore::Server(config, FLAGS_group_idx,
          FLAGS_replica_idx, FLAGS_num_shards, FLAGS_num_groups,
          tport, &keyManager, params, timeDelta, indicusOCCType, part, FLAGS_indicus_sig_batch_timeout);
      break;
    }
		case PROTO_PBFT: {
			server = new pbftstore::Server(config, &keyManager,
				FLAGS_group_idx, FLAGS_replica_idx, FLAGS_num_shards, FLAGS_num_groups,
				FLAGS_indicus_sign_messages, FLAGS_indicus_validate_proofs, FLAGS_indicus_time_delta,
				part);
			replica = new pbftstore::Replica(config, &keyManager,
				dynamic_cast<pbftstore::App *>(server),
				FLAGS_group_idx, FLAGS_replica_idx, FLAGS_indicus_sign_messages, 1, false,
			 	tport);
			break;
		}
    default: {
      NOT_REACHABLE();
    }
  }

  // parse keys
  std::vector<std::string> keys;
  if (FLAGS_data_file_path.empty() && FLAGS_keys_path.empty()) {
    /*if (FLAGS_num_keys > 0) {
      for (size_t i = 0; i < FLAGS_num_keys; ++i) {
        keys.push_back(std::to_string(i));
      }
    } else {
      std::cerr << "Specified neither keys file nor number of keys."
                << std::endl;
      return 1;
    }*/
  } else if (FLAGS_data_file_path.length() > 0 && FLAGS_keys_path.empty()) {
    std::ifstream in;
    in.open(FLAGS_data_file_path);
    if (!in) {
      std::cerr << "Could not read data from: " << FLAGS_data_file_path
                << std::endl;
      return 1;
    }
    size_t loaded = 0;
    size_t stored = 0;
    Debug("Populating with data from %s.", FLAGS_data_file_path.c_str());
    std::vector<int> txnGroups;
    while (!in.eof()) {
      std::string key;
      std::string value;
      int i = ReadBytesFromStream(&in, key);
      if (i == 0) {
        ReadBytesFromStream(&in, value);
        if ((*part)(key, FLAGS_num_shards, FLAGS_group_idx, txnGroups) % FLAGS_num_groups == FLAGS_group_idx) {
          server->Load(key, value, Timestamp());
          ++stored;
        }
        ++loaded;
      }
    }
    Debug("Stored %lu out of %lu key-value pairs from file %s.", stored,
        loaded, FLAGS_data_file_path.c_str());
  } else {
    std::ifstream in;
    in.open(FLAGS_keys_path);
    if (!in) {
      std::cerr << "Could not read keys from: " << FLAGS_keys_path
                << std::endl;
      return 1;
    }
    std::string key;
    std::vector<int> txnGroups;
    while (std::getline(in, key)) {
      if ((*part)(key, FLAGS_num_shards, FLAGS_group_idx, txnGroups) % FLAGS_num_groups == FLAGS_group_idx) {
        server->Load(key, "null", Timestamp());
      }
    }
    in.close();
  }

  std::signal(SIGKILL, Cleanup);
  std::signal(SIGTERM, Cleanup);
  std::signal(SIGINT, Cleanup);

  CALLGRIND_START_INSTRUMENTATION;
  tport->Run();
  CALLGRIND_STOP_INSTRUMENTATION;
  CALLGRIND_DUMP_STATS;

  return 0;
}

void Cleanup(int signal) {
  if (FLAGS_stats_file.size() > 0) {
    server->GetStats().ExportJSON(FLAGS_stats_file);
  }
  delete server;
  delete part;
  if (replica != nullptr) {
    delete replica;
  }
  if (tport != nullptr) {
    tport->Stop(true);
    delete tport;
  }
  exit(0);
}
