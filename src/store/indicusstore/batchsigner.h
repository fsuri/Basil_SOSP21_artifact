#ifndef BATCH_SIGNER_H
#define BATCH_SIGNER_H

#include <functional>
#include <vector>

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
      bool adjustBatchSize) : transport(transport), keyManager(keyManager),
      stats(stats), batchTimeoutMicro(batchTimeoutMicro),
      initialBatchSize(batchSize), id(id), adjustBatchSize(adjustBatchSize) { }
  virtual ~BatchSigner() { }

  virtual void MessageToSign(::google::protobuf::Message* msg,
      proto::SignedMessage *signedMessage, signedCallback cb,
      bool finishBatch = false) = 0;

 protected:
  Transport *transport;
  KeyManager *keyManager;
  Stats &stats;
  const unsigned int batchTimeoutMicro;
  const uint64_t initialBatchSize;
  const uint64_t id;
  const bool adjustBatchSize;

};

} // namespace indicusstore

#endif /* BATCH_SIGNER_H */
