#include "store/pbftstore/replica.h"
#include "store/pbftstore/server.h"

int main(int argc, char **argv) {
  const char *configPath = NULL;
  const char *keyPath = NULL;
  int groupIdx = -1;
  int myId = -1;

  // Parse arguments
  int opt;
  char *strtolPtr;
  while ((opt = getopt(argc, argv, "c:k:g:i:")) != -1) {
    switch (opt) {
      case 'c':
        configPath = optarg;
        break;
      case 'k':
        keyPath = optarg;
        break;
      case 'g': {
        groupIdx = strtoul(optarg, &strtolPtr, 10);
        if ((*optarg == '\0') || (*strtolPtr != '\0') || (groupIdx < 0)) {
          fprintf(stderr, "option -g requires a numeric arg\n");
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

  if (!configPath) {
    fprintf(stderr, "option -c is required\n");
    exit(-1);
  }
  if (!keyPath) {
    fprintf(stderr, "option -k is required\n");
    exit(-1);
  }
  if (groupIdx == -1) {
    fprintf(stderr, "option -g is required\n");
    exit(-1);
  }
  if (myId == -1) {
    fprintf(stderr, "option -i is required\n");
    exit(-1);
  }

  std::ifstream configStream(configPath);
  if (configStream.fail()) {
    fprintf(stderr, "unable to read configuration file: %s\n",
            configPath);
    exit(1);
  }
  transport::Configuration config(configStream);

  UDPTransport transport(0.0, 0.0, 0);

  pbftstore::Server* server = new pbftstore::Server();
  KeyManager keyManager(keyPath);
  pbftstore::Replica replica(config, &keyManager, dynamic_cast<pbftstore::App *>(server), groupIdx, myId, false, &transport);

  transport.Run();

  return 0;
}
