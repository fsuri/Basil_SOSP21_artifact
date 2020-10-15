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
