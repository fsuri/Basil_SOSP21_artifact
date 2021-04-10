#ifndef RW_AUGUSTUS_CLIENT_H
#define RW_AUGUSTUS_CLIENT_H

#include "store/benchmark/async/async_transaction_bench_client.h"
#include "store/benchmark/async/rw_augustus/rw_transaction.h"
#include "store/benchmark/async/common/key_selector.h"

namespace rw_augustus {

enum KeySelection {
  UNIFORM,
  ZIPF
};

class RWAugustusClient : public AsyncTransactionBenchClient {
 public:
  RWAugustusClient(KeySelector *keySelector, uint64_t numKeys, AsyncClient &client,
      Transport &transport, uint64_t id, int numRequests, int expDuration,
      uint64_t delay, int warmupSec, int cooldownSec, int tputInterval,
      uint32_t abortBackoff, bool retryAborted, uint32_t maxBackoff, uint32_t maxAttempts,
      const std::string &latencyFilename = "");

  virtual ~RWAugustusClient();

 protected:
  virtual AsyncTransaction *GetNextTransaction();
  virtual std::string GetLastOp() const;

 private:
  KeySelector *keySelector; 
  uint64_t numKeys;
  uint64_t tid = 0;
};

} //namespace rw_augustus

#endif /* RW_AUGUSTUS_CLIENT_H */
