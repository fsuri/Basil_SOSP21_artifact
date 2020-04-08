#ifndef RETWIS_CLIENT_H
#define RETWIS_CLIENT_H

#include "store/benchmark/async/async_transaction_bench_client.h"
#include "store/benchmark/async/retwis/retwis_transaction.h"
#include "store/benchmark/async/common/key_selector.h"

namespace retwis {

enum KeySelection {
  UNIFORM,
  ZIPF
};

class RetwisClient : public AsyncTransactionBenchClient {
 public:
  RetwisClient(KeySelector *keySelector, AsyncClient &client,
      Transport &transport, uint32_t clientId, int numRequests, int expDuration,
      uint64_t delay, int warmupSec, int cooldownSec, int tputInterval,
      uint32_t abortBackoff,
      bool retryAborted, int32_t maxAttempts, uint32_t seed,
      const std::string &latencyFilename = "");

  virtual ~RetwisClient();

 protected:
  virtual AsyncTransaction *GetNextTransaction();
  virtual std::string GetLastOp() const;

 private:
  KeySelector *keySelector; 
  std::string lastOp;
};

} //namespace retwis

#endif /* RETWIS_CLIENT_H */
