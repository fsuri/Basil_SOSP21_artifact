
#ifndef _NODE_CLI_H_
#define _NODE_CLI_H_

#include <string>
#include <utility>

#include "bft_tapir/config.h"

namespace bft_tapir {

std::pair<NodeConfig, int> parseCLI(int argc, char **argv) {
  const char *replicaConfigPath = NULL;
  const char *clientConfigPath = NULL;
  const char *keyPath = NULL;
  int n = -1;
  int f = -1;
  int c = -1;
  int myId = -1;

  // Parse arguments
  int opt;
  char *strtolPtr;
  while ((opt = getopt(argc, argv, "r:c:k:n:f:m:i:")) != -1) {
    switch (opt) {
      case 'r':
        replicaConfigPath = optarg;
        break;
      case 'c':
        clientConfigPath = optarg;
        break;
      case 'k':
        keyPath = optarg;
        break;
      case 'n': {
        n = strtoul(optarg, &strtolPtr, 10);
        if ((*optarg == '\0') || (*strtolPtr != '\0') || (n < 0)) {
          fprintf(stderr, "option -n requires a numeric arg\n");
        }
        break;
      }
      case 'f': {
        f = strtoul(optarg, &strtolPtr, 10);
        if ((*optarg == '\0') || (*strtolPtr != '\0') || (f < 0)) {
          fprintf(stderr, "option -f requires a numeric arg\n");
        }
        break;
      }
      case 'm': {
        c = strtoul(optarg, &strtolPtr, 10);
        if ((*optarg == '\0') || (*strtolPtr != '\0') || (c < 0)) {
          fprintf(stderr, "option -m requires a numeric arg\n");
        }
        break;
      }
      case 'i': {
        myId = strtoul(optarg, &strtolPtr, 10);
        if ((*optarg == '\0') || (*strtolPtr != '\0') || (myId < 0)) {
          fprintf(stderr, "option -i requires a numeric arg\n");
        }
        break;
      }
      default:
        fprintf(stderr, "Unknown argument %s\n", argv[optind]);
    }
  }

  if (!replicaConfigPath) {
    fprintf(stderr, "option -r is required\n");
    exit(-1);
  }
  if (!clientConfigPath) {
    fprintf(stderr, "option -c is required\n");
    exit(-1);
  }
  if (!keyPath) {
    fprintf(stderr, "option -k is required\n");
    exit(-1);
  }
  if (n == -1) {
    fprintf(stderr, "option -n is required\n");
    exit(-1);
  }
  if (f == -1) {
    fprintf(stderr, "option -f is required\n");
    exit(-1);
  }
  if (c == -1) {
    fprintf(stderr, "option -m is required\n");
    exit(-1);
  }
  if (myId == -1) {
    fprintf(stderr, "option -i is required\n");
    exit(-1);
  }

  std::ifstream replicaConfigStream(replicaConfigPath);
  if (replicaConfigStream.fail()) {
    fprintf(stderr, "unable to read configuration file: %s\n",
            replicaConfigPath);
    exit(1);
  }
  transport::Configuration replicaConfig(replicaConfigStream);

  std::ifstream clientConfigStream(clientConfigPath);
  if (clientConfigStream.fail()) {
    fprintf(stderr, "unable to read configuration file: %s\n",
            clientConfigPath);
    exit(1);
  }
  transport::Configuration clientConfig(clientConfigStream);

  NodeConfig config(replicaConfig, clientConfig, std::string(keyPath), n, f, c);
  return std::pair<NodeConfig, int>(config, myId);
}
}  // namespace bft_tapir

#endif /* _NODE_CLI_H_ */