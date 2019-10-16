#ifndef RW_CLIENT_H
#define RW_CLIENT_H

#include "store/benchmark/async/async_transaction_bench_client.h"
#include "store/benchmark/async/rw/rw_transaction.h"
#include "store/benchmark/async/common/key_selector.h"

namespace rw {

enum KeySelection {
  UNIFORM,
  ZIPF
};

class RWClient : public AsyncTransactionBenchClient {
 public:
  RWClient(KeySelector *keySelector, uint64_t numKeys, AsyncClient &client,
      Transport &transport, int numRequests, int expDuration, uint64_t delay,
      int warmupSec, int cooldownSec, int tputInterval, uint32_t abortBackoff,
      bool retryAborted, const std::string &latencyFilename = "");

  virtual ~RWClient();

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
