/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fsp@cs.cornell.edu>
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
      Transport &transport, uint64_t id, int numRequests, int expDuration,
      uint64_t delay, int warmupSec, int cooldownSec, int tputInterval,
      uint32_t abortBackoff, bool retryAborted, uint32_t maxBackoff, uint32_t maxAttempts,
      const std::string &latencyFilename = "");

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
