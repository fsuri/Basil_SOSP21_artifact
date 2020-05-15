#include "store/indicusstore/batchsigner.h"

#include "store/indicusstore/common.h"

namespace indicusstore {

BatchSigner::BatchSigner(Transport *transport, KeyManager *keyManager, Stats &stats,
    uint64_t batchTimeoutMicro, uint64_t batchSize, uint64_t id,
    bool adjustBatchSize) :
    transport(transport), keyManager(keyManager), stats(stats),
    batchTimeoutMicro(batchTimeoutMicro), initialBatchSize(batchSize),
    id(id), adjustBatchSize(adjustBatchSize), 
    batchTimerRunning(false),
    batchSize(batchSize),
    messagesBatchedInterval(0UL) {
  if (adjustBatchSize) {
    transport->TimerMicro(batchTimeoutMicro, std::bind(
        &BatchSigner::AdjustBatchSize, this));
  }
}

BatchSigner::~BatchSigner() {
}

void BatchSigner::MessageToSign(::google::protobuf::Message* msg,
    proto::SignedMessage *signedMessage, signedCallback cb, bool finishBatch) {
  if (initialBatchSize == 1) {
    Debug("Initial batch size = 1, immediately signing");
    SignMessage(msg, keyManager->GetPrivateKey(id), id,
        signedMessage);
    cb();
  } else {
    messagesBatchedInterval++;
    pendingBatchMessages.push_back(msg);
    pendingBatchSignedMessages.push_back(signedMessage);
    pendingBatchCallbacks.push_back(cb);

    if (finishBatch || pendingBatchMessages.size() >= batchSize) {
      Debug("Batch is full, sending");
      if (batchTimerRunning) {
        transport->CancelTimer(batchTimerId);
        batchTimerRunning = false;
      }
      SignBatch();
    } else if (!batchTimerRunning) {
      batchTimerRunning = true;
      Debug("Starting batch timer");
      batchTimerId = transport->TimerMicro(batchTimeoutMicro, [this]() {
        Debug("Batch timer expired with %lu items, sending",
            this->pendingBatchMessages.size());
        this->batchTimerRunning = false;
        this->SignBatch();
      });
    }
  }
}
void BatchSigner::SignBatch() {
  stats.IncrementList("sig_batch", pendingBatchMessages.size());
  SignMessages(pendingBatchMessages, keyManager->GetPrivateKey(id), id,
    pendingBatchSignedMessages);
  pendingBatchMessages.clear();
  pendingBatchSignedMessages.clear();
  for (const auto& cb : pendingBatchCallbacks) {
    cb();
  }
  pendingBatchCallbacks.clear();
}

void BatchSigner::AdjustBatchSize() {
  batchSize = (0.75 * batchSize) + (0.25 * messagesBatchedInterval);
  messagesBatchedInterval = 0;
  transport->TimerMicro(batchTimeoutMicro, std::bind(&BatchSigner::AdjustBatchSize,
        this));
}

} // namespace indicusstore
