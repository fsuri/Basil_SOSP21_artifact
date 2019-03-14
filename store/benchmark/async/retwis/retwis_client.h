#ifndef RETWIS_CLIENT_H
#define RETWIS_CLIENT_H

#include "store/benchmark/async/bench_client.h"
#include "store/benchmark/async/retwis/retwis_transaction.h"
#include "store/benchmark/async/common/key_selector.h"

namespace retwis {

enum KeySelection {
  UNIFORM,
  ZIPF
};

class RetwisClient : public BenchmarkClient {
 public:
  RetwisClient(KeySelector *keySelector, Client &client, Transport &transport,
      int numRequests, uint64_t delay, int warmupSec, int tputInterval,
      const std::string &latencyFilename = "");

  virtual ~RetwisClient();

 protected:
  virtual void SendNext();

 private:
  KeySelector *keySelector; 
  RetwisTransaction *currTxn;
};

} //namespace retwis

#endif /* RETWIS_CLIENT_H */
