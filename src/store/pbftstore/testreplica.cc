/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fs435@cornell.edu>
 *                Matthew Burke <matthelb@cs.cornell.edu>
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

  KeyManager keyManager(keyPath, crypto::ED25, true);
  int numShards = 1;
  int numGroups = 1;
  uint64_t maxBatchSize = 3;
  bool primaryCoordinator = false;
  bool signMessages = true;
  bool validateProofs = true;
  uint64_t EbatchSize = 1;
  uint64_t EbatchTimeoutMS = 10;
  uint64_t timeoutms = 10;
  DefaultPartitioner dp;
  pbftstore::Server* server = new pbftstore::Server(config, &keyManager, groupIdx, myId, numShards, numGroups, signMessages, validateProofs, 10, &dp);
  pbftstore::Replica replica(config, &keyManager, dynamic_cast<pbftstore::App *>(server), groupIdx, myId, signMessages, maxBatchSize, timeoutms, EbatchSize, EbatchTimeoutMS, primaryCoordinator, false, &transport);

  printf("Running transport\n");
  transport.Run();

  return 0;
}
