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
#ifndef BATCH_SIGNER_H
#define BATCH_SIGNER_H

#include <functional>
#include <vector>
#include <mutex>

#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>

#include "lib/transport.h"
#include "lib/keymanager.h"
#include "store/indicusstore/indicus-proto.pb.h"
#include "store/indicusstore/common.h"
#include "store/common/stats.h"
#include "lib/latency.h"

namespace indicusstore {

class BatchSigner {
 public:
  BatchSigner(Transport *transport, KeyManager *keyManager, Stats &stats,
      uint64_t batchTimeoutMicro, uint64_t batchSize, uint64_t id,
      bool adjustBatchSize, uint64_t merkleBranchFactor) : transport(transport), keyManager(keyManager),
      stats(stats), batchTimeoutMicro(batchTimeoutMicro),
      initialBatchSize(batchSize), id(id), adjustBatchSize(adjustBatchSize),
      merkleBranchFactor(merkleBranchFactor) { }
  virtual ~BatchSigner() { }

  virtual void MessageToSign(::google::protobuf::Message* msg,
      proto::SignedMessage *signedMessage, signedCallback cb,
      bool finishBatch = false) = 0;

  virtual void asyncMessageToSign(::google::protobuf::Message* msg,
      proto::SignedMessage *signedMessage, signedCallback cb,
      bool finishBatch = false) = 0;

  std::mutex batchMutex;
  //Latency_t waitOnBatchLock;

 protected:
  Transport *transport;
  KeyManager *keyManager;
  Stats &stats;
  const unsigned int batchTimeoutMicro;
  const uint64_t initialBatchSize;
  const uint64_t id;
  const bool adjustBatchSize;
  const uint64_t merkleBranchFactor;




};

} // namespace indicusstore

#endif /* BATCH_SIGNER_H */
