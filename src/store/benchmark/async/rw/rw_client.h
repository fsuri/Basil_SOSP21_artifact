#ifndef RW_CLIENT_H
#define RW_CLIENT_H

#include "store/benchmark/async/async_transaction_bench_client.h"
#include "store/benchmark/async/rw/rw_transaction.h"
#include "store/benchmark/async/common/key_selector.h"
#include <unordered_map>

namespace rw {

enum KeySelection {
  UNIFORM,
  ZIPF
};

class RWClient : public AsyncTransactionBenchClient {
 public:
  RWClient(KeySelector *keySelector, uint64_t numKeys, AsyncClient &client,
      Transport &transport, uint64_t id, int numRequests, int expDuration,
      uint64_t delay, int warmupSec, int cooldownSec, int tputInterval,
      uint32_t abortBackoff, bool retryAborted, uint32_t maxBackoff, uint32_t maxAttempts,
      const std::string &latencyFilename = "");

  virtual ~RWClient();

  std::unordered_map<int, int> key_counts;

 protected:
  virtual AsyncTransaction *GetNextTransaction();
  virtual std::string GetLastOp() const;

 private:
  KeySelector *keySelector;
  uint64_t numKeys;
  uint64_t tid = 0;
};

} //namespace rw

#endif /* RW_CLIENT_H */
