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

#include "lib/tcptransport.h"
#include "lib/io_utils.h"

#include "store/common/frontend/client.h"
#include "store/server.h"
#include "store/strongstore/server.h"
#include "store/tapirstore/server.h"
#include "store/weakstore/server.h"

#include <gflags/gflags.h>

enum protocol_t {
	PROTO_UNKNOWN,
	PROTO_TAPIR,
	PROTO_WEAK,
	PROTO_STRONG
};

/**
 * System settings.
 */
DEFINE_string(config_path, "", "path to shard configuration file");
DEFINE_uint64(replica_idx, 0, "index of replica in shard configuration file");
DEFINE_uint64(shard_idx, 0, "index of shard to which this replica belongs");
DEFINE_uint64(num_shards, 1, "number of shards in the system");

const std::string protocol_args[] = {
	"tapir",
  "weak",
  "strong"
};
const protocol_t protos[] {
  PROTO_TAPIR,
  PROTO_WEAK,
  PROTO_STRONG
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

/**
 * TAPIR settings.
 */
DEFINE_bool(linearizable, true, "run TAPIR in linearizable mode");

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
 * Experiment settings.
 */
DEFINE_int32(clock_skew, 0, "difference between real clock and TrueTime");
DEFINE_int32(clock_error, 0, "maximum error for clock");

/**
 * Benchmark settings.
 */
DEFINE_string(keys_path, "", "path to file containing keys in the system");
DEFINE_uint64(num_keys, 0, "number of keys to generate");
DEFINE_string(data_file_path, "", "path to file containing key-value pairs to be loaded");

int main(int argc, char **argv) {
  gflags::SetUsageMessage(
           "runs a replica for a distributed replicated transaction\n"
"           processing system.");
	gflags::ParseCommandLineFlags(&argc, &argv, true);

  // parse configuration
  std::ifstream configStream(FLAGS_config_path);
  if (configStream.fail()) {
    std::cerr << "Unable to read configuration file: " << FLAGS_config_path
              << std::endl;
  }
  transport::Configuration config(configStream);

  if (FLAGS_replica_idx >= config.n) {
    std::cerr << "Replica index " << FLAGS_replica_idx << " is out of bounds"
                 "; only " << config.n << " replicas defined" << std::endl;
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


  UDPTransport transport(0.0, 0.0, 0);

  ::Server *server;
  TransportReceiver *replica = nullptr;

  switch (proto) {
    case PROTO_TAPIR: {
      server = new tapirstore::Server(FLAGS_linearizable);
      replica = new replication::ir::IRReplica(config, FLAGS_replica_idx,
          &transport, dynamic_cast<replication::ir::IRAppReplica *>(server));
      break;
    }
    case PROTO_WEAK: {
      server = new weakstore::Server(config, FLAGS_replica_idx, &transport);
      break;
    }
    case PROTO_STRONG: {
      server = new strongstore::Server(strongMode, FLAGS_clock_skew,
          FLAGS_clock_error);
      replica = new replication::vr::VRReplica(config, FLAGS_replica_idx,
          &transport, 1, dynamic_cast<replication::AppReplica *>(server));
      break;
    }
    default: {
      NOT_REACHABLE();
    }
  }

  // parse keys
  std::vector<std::string> keys;
  if (FLAGS_data_file_path.empty() && FLAGS_keys_path.empty()) {
    if (FLAGS_num_keys > 0) {
      for (size_t i = 0; i < FLAGS_num_keys; ++i) {
        keys.push_back(std::to_string(i));
      }
    } else {
      std::cerr << "Specified neither keys file nor number of keys."
                << std::endl;
      return 1;
    }
  } else if (FLAGS_data_file_path.length() > 0 && FLAGS_keys_path.empty()) {
    std::ifstream in;
    in.open(FLAGS_data_file_path);
    if (!in) {
      std::cerr << "Could not read data from: " << FLAGS_data_file_path
                << std::endl;
      return 1;
    }
    std::cerr << "Loaded 0 key-value pairs.";
    size_t loaded = 0;
    while (!in.eof()) {
      std::string key;
      std::string value;
      int i = ReadBytesFromStream(&in, key);
      if (i == 0) {
        ReadBytesFromStream(&in, value);
        server->Load(key, value, Timestamp());
      }
      loaded++;
      std::cerr << "\rLoaded " << loaded << " key-value pairs.";
    }
    std::cerr << std::endl;
    Debug("Loaded all data from file %s.", FLAGS_data_file_path.c_str());
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
      if (Client::key_to_shard(key, FLAGS_num_shards) == FLAGS_shard_idx) {
        server->Load(key, "null", Timestamp());
      }
    }
    in.close();
  }

  transport.Run();
  delete server;
  if (replica != nullptr) {
    delete replica;
  }
  return 0;
}
