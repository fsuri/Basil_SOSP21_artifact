#ifndef LOCAL_BATCH_SIGNER_H
#define LOCAL_BATCH_SIGNER_H

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
#include "store/indicusstore/batchsigner.h"

namespace indicusstore {

class LocalBatchSigner : public BatchSigner {
 public:
  LocalBatchSigner(Transport *transport, KeyManager *keyManager, Stats &stats,
      uint64_t batchTimeoutMicro, uint64_t batchSize, uint64_t id,
      bool adjustBatchSize);
  virtual ~LocalBatchSigner();

  virtual void MessageToSign(::google::protobuf::Message* msg,
      proto::SignedMessage *signedMessage, signedCallback cb,
      bool finishBatch = false) override;

 private:
  void SignBatch();
  void AdjustBatchSize();

  bool batchTimerRunning;
  uint64_t batchSize;
  uint64_t messagesBatchedInterval;

  int batchTimerId;
  std::vector<::google::protobuf::Message*> pendingBatchMessages;
  std::vector<proto::SignedMessage*> pendingBatchSignedMessages;
  std::vector<signedCallback> pendingBatchCallbacks;

};

} // namespace indicusstore

#endif /* LOCAL_BATCH_SIGNER_H */
