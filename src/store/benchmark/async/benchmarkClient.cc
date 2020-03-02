// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * store/benchmark/tpccClient.cc:
 *   Benchmarking client for tpcc.
 *
 **********************************************************************/

#include "lib/latency.h"
#include "lib/timeval.h"
#include "store/common/truetime.h"
#include "store/common/frontend/client.h"
#include "store/strongstore/client.h"
#include "store/weakstore/client.h"
#include "store/tapirstore/client.h"
#include "store/benchmark/async/tpcc/new_order.h"

#include <vector>
#include <algorithm>

enum phase_t {
	WARMUP,
	MEASURE,
	COOLDOWN
};

enum protomode_t {
	PROTO_UNKNOWN,
	MODE_TAPIR,
	MODE_WEAK,
	MODE_STRONG
};

DEFINE_LATENCY(op);

int main(int argc, char **argv) {
	const char * configPath = nullptr;
	int duration = 10;
	int nshards = 1;
	int warehouse_per_shard = 1;
	int clients_per_warehouse = 1;
	int client_id = 0;
	int remote_item_milli_p = -1;
	int total_warehouses;
	int ops_per_iteration = 1;
  int closestReplica = -1; // Closest replica id.
  int skew = 0; // difference between real clock and TrueTime
  int error = 0; // error bars

	struct timeval startTime, endTime, initialTime, currTime;
	struct Latency_t latency;
  std::vector<uint64_t> latencies;

  Client *protoClient = nullptr;

  phase_t phase = WARMUP;
  protomode_t mode = PROTO_UNKNOWN;
  // Mode for strongstore.
  strongstore::Mode strongmode;

	int opt;
	while ((opt = getopt(argc, argv, "c:d:s:w:p:i:r:o:m:e:k:")) != -1) {
		switch (opt) {
		case 'c': {
			configPath = optarg;
			break;
		}
		case 'd': {
			char *strtolPtr;
			duration = strtoul(optarg, &strtolPtr, 10);
			if ((*optarg == '\0' || (*strtolPtr != '\0') || (duration <= 0))) {
				fprintf(stderr, "option -d requires a numeric arg > 0\n");
			}
			break;
		}
		case 's': {
			char *strtolPtr;
			nshards = strtoul(optarg, &strtolPtr, 10);
			if ((*optarg == '\0' || (*strtolPtr != '\0') || (nshards <= 0))) {
				fprintf(stderr, "option -s requires a numeric arg > 0\n");
			}
			break;
		}
    case 'k': { // Simulated clock skew.
      char *strtolPtr;
      skew = strtoul(optarg, &strtolPtr, 10);
      if ((*optarg == '\0') || (*strtolPtr != '\0') || (skew < 0)) {
        fprintf(stderr, "option -k requires a numeric arg\n");
      }
      break;
    }
    case 'e': { // Simulated clock error
      char *strtolPtr;
      error = strtoul(optarg, &strtolPtr, 10);
      if ((*optarg == '\0') || (*strtolPtr != '\0') || (error < 0)) {
        fprintf(stderr, "option -e requires a numeric arg\n");
      }
      break;
    }

		case 'w': {
			char *strtolPtr;
			warehouse_per_shard = strtoul(optarg, &strtolPtr, 10);
			if ((*optarg == '\0' || *strtolPtr != '\0' || warehouse_per_shard <= 0)) {
				fprintf(stderr, "option -w requires a numeric arg > 0\n");
			}
			break;
		}
		case 'p': {
			char *strtolPtr;
			clients_per_warehouse = strtoul(optarg, &strtolPtr, 10);
			if ((*optarg == '\0' || *strtolPtr != '\0' || clients_per_warehouse <= 0)) {
				fprintf(stderr, "option -p requires a numeric arg > 0\n");
			}
			break;
		}
		case 'i': {
			char *strtolPtr;
			client_id = strtoul(optarg, &strtolPtr, 10);
			if ((*optarg == '\0' || *strtolPtr != '\0' || client_id < 0)) {
				fprintf(stderr, "option -i requires a numeric arg >= 0");
			}
			break;
		}
		case 'r': {
			char *strtolPtr;
			remote_item_milli_p = strtoul(optarg, &strtolPtr, 10);
			if ((*optarg == '\0' || *strtolPtr != '\0' || remote_item_milli_p < -1)) {
				fprintf(stderr, "option -r requires a numeric arg >= -1");
			}
			break;
		}
		case 'o': {
			char *strtolPtr;
			ops_per_iteration = strtoul(optarg, &strtolPtr, 10);
			if ((*optarg == '\0' || *strtolPtr != '\0' || ops_per_iteration <= 0)) {
				fprintf(stderr, "option -o requires a numeric arg > 0");
			}
			break;
		}
    case 'm': {
      if (strcasecmp(optarg, "txn-l") == 0) {
        mode = MODE_TAPIR;
      } else if (strcasecmp(optarg, "txn-s") == 0) {
        mode = MODE_TAPIR;
      } else if (strcasecmp(optarg, "qw") == 0) {
        mode = MODE_WEAK;
      } else if (strcasecmp(optarg, "occ") == 0) {
        mode = MODE_STRONG;
        strongmode = strongstore::MODE_OCC;
      } else if (strcasecmp(optarg, "lock") == 0) {
        mode = MODE_STRONG;
        strongmode = strongstore::MODE_LOCK;
      } else if (strcasecmp(optarg, "span-occ") == 0) {
        mode = MODE_STRONG;
        strongmode = strongstore::MODE_SPAN_OCC;
      } else if (strcasecmp(optarg, "span-lock") == 0) {
        mode = MODE_STRONG;
        strongmode = strongstore::MODE_SPAN_LOCK;
      } else {
        fprintf(stderr, "unknown mode '%s'\n", optarg);
        exit(0);
      }
      break;
    }
		default:
			fprintf(stderr, "Unkown argument %s\n", argv[optind]);
			break;
		}
	}

	if (configPath == nullptr) {
		Panic("tpccClient requires -c option\n");
	}

  if (mode == PROTO_UNKNOWN) {
    Panic("tpccClient requires -m option\n");
  }

	total_warehouses = nshards * warehouse_per_shard;

  switch (mode) {
  	case MODE_TAPIR: {
			protoClient = new tapirstore::Client(configPath, nshards, closestReplica,
		      TrueTime(skew, error));
      break;
    }
		case MODE_WEAK: {
			protoClient = new weakstore::Client(configPath, nshards, closestReplica);
			break;
		}
		case MODE_STRONG: {
      protoClient = new strongstore::Client(strongmode, configPath, nshards,
          closestReplica, TrueTime(skew, error));
			break;
		}
		default:
			Panic("Unknown protocol mode");
	}
  
	latencies.reserve(duration * 10000);
	_Latency_Init(&latency, "op");


	gettimeofday(&initialTime, NULL);
	while (true) {
		gettimeofday(&currTime, NULL);
		uint64_t time_elapsed = currTime.tv_sec - initialTime.tv_sec;

		if (phase == WARMUP) {
			if (time_elapsed >= (uint64_t)duration / 3) {
				phase = MEASURE;
				startTime = currTime;
			}
		} else if (phase == MEASURE) {
			if (time_elapsed >= (uint64_t)duration * 2 / 3) {
				phase = COOLDOWN;
				endTime = currTime;
			}
		} else if (phase == COOLDOWN) {
			if (time_elapsed >= (uint64_t)duration) {
				break;
			}
		}

    tpcc::NewOrder txn;
		Latency_Start(&latency);
    protoClient->Begin();
    while (!txn.IsFinished()) {
      txn.ExecuteNextOperation(protoClient);
    }
		uint64_t ns = Latency_End(&latency);
    int num_new_orders = 1;

		if (phase == MEASURE) {
      if (num_new_orders > 0) {
			  latencies.push_back(ns);
      }
		}
	}

	struct timeval diff = timeval_sub(endTime, startTime);

	Notice("Completed %lu transactions in " FMT_TIMEVAL_DIFF " seconds",
	       latencies.size(), VA_TIMEVAL_DIFF(diff));

	char buf[1024];
	std::sort(latencies.begin(), latencies.end());

	uint64_t ns  = latencies[latencies.size() / 2];
	LatencyFmtNS(ns, buf);
	Notice("Median latency is %ld ns (%s)", ns, buf);

	ns = latencies[latencies.size() * 90 / 100];
	LatencyFmtNS(ns, buf);
	Notice("90th percentile latency is %ld ns (%s)", ns, buf);

	ns = latencies[latencies.size() * 95 / 100];
	LatencyFmtNS(ns, buf);
	Notice("95th percentile latency is %ld ns (%s)", ns, buf);

	ns = latencies[latencies.size() * 99 / 100];
	LatencyFmtNS(ns, buf);
	Notice("99th percentile latency is %ld ns (%s)", ns, buf);

  if (protoClient) {
    delete protoClient;
  }
	return 0;
}

