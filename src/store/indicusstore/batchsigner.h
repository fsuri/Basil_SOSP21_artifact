#ifndef BATCH_SIGNER_H
#define BATCH_SIGNER_H

#include <functional>
#include <vector>

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
      bool adjustBatchSize);
  virtual ~BatchSigner();

  void MessageToSign(::google::protobuf::Message* msg,
      proto::SignedMessage *signedMessage, signedCallback cb,
      bool finishBatch = false);

 private:
  void SignBatch();
  void AdjustBatchSize();

  Transport *transport;
  KeyManager *keyManager;
  Stats &stats;
  const unsigned int batchTimeoutMicro;
  const uint64_t initialBatchSize;
  const uint64_t id;
  const bool adjustBatchSize;
  bool batchTimerRunning;
  uint64_t batchSize;
  uint64_t messagesBatchedInterval;

  int batchTimerId;
  std::vector<::google::protobuf::Message*> pendingBatchMessages;
  std::vector<proto::SignedMessage*> pendingBatchSignedMessages;
  std::vector<signedCallback> pendingBatchCallbacks;

};

} // namespace indicusstore

#endif /* BATCH_SIGNER_H */
