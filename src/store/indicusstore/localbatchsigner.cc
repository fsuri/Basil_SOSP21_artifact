#include "store/indicusstore/localbatchsigner.h"

#include "store/indicusstore/common.h"

namespace indicusstore {

LocalBatchSigner::LocalBatchSigner(Transport *transport, KeyManager *keyManager, Stats &stats,
    uint64_t batchTimeoutMicro, uint64_t batchSize, uint64_t id,
    bool adjustBatchSize, uint64_t merkleBranchFactor) : BatchSigner(transport, keyManager, stats,
      batchTimeoutMicro, batchSize, id, adjustBatchSize, merkleBranchFactor),
    batchTimerRunning(false),
    batchSize(batchSize),
    messagesBatchedInterval(0UL) {
  if (adjustBatchSize) {
    transport->TimerMicro(batchTimeoutMicro, std::bind(
        &LocalBatchSigner::AdjustBatchSize, this));
  }
}

LocalBatchSigner::~LocalBatchSigner() {
}

void LocalBatchSigner::MessageToSign(::google::protobuf::Message* msg,
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
void LocalBatchSigner::SignBatch() {
  uint64_t batchSize = pendingBatchMessages.size();
  stats.IncrementList("sig_batch", batchSize);
  stats.Add("sig_batch_sizes", batchSize);
  struct timeval curr;
  gettimeofday(&curr, NULL);
  uint64_t currMicros = curr.tv_sec * 1000000ULL + curr.tv_usec;
  stats.Add("sig_batch_sizes_ts",  currMicros);
  SignMessages(pendingBatchMessages, keyManager->GetPrivateKey(id), id,
    pendingBatchSignedMessages, merkleBranchFactor);
  pendingBatchMessages.clear();
  pendingBatchSignedMessages.clear();
  for (const auto& cb : pendingBatchCallbacks) {
    cb();
  }
  pendingBatchCallbacks.clear();
}

void LocalBatchSigner::AdjustBatchSize() {
  batchSize = (0.75 * batchSize) + (0.25 * messagesBatchedInterval);
  messagesBatchedInterval = 0;
  transport->TimerMicro(batchTimeoutMicro, std::bind(&LocalBatchSigner::AdjustBatchSize,
        this));
}



//Main calls DispatchTP(asyncMessageToSign, ManageCallbacks) asyncMessageToSign will call asyncSignBatch


void LocalBatchSigner::asyncMessageToSign(::google::protobuf::Message* msg,
    proto::SignedMessage *signedMessage, signedCallback cb, bool finishBatch) {
  //acquire lock.
  // std::unique_lock<std::mutex> lk(verifierMutex);
  // cv.wait(lk, [] { return !signing; });
  //declare return var
  // bool* ret = new bool;

  if (initialBatchSize == 1) {
    Debug("Initial batch size = 1, immediately signing");

    std::function<void*()> f(std::bind(asyncSignMessage, msg, keyManager->GetPrivateKey(id), id, signedMessage));
    transport->DispatchTP(f, [cb](void * ret){ cb();});

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

      std::function<void*()> f(std::bind(&LocalBatchSigner::asyncSignBatch, this,
        pendingBatchMessages, pendingBatchSignedMessages, pendingBatchCallbacks));

      pendingBatchMessages.clear();
      pendingBatchSignedMessages.clear();
      pendingBatchCallbacks.clear();
      transport->DispatchTP(f, [](void* ret){delete (bool*) ret;});


    } else if (!batchTimerRunning) {
      batchTimerRunning = true;
      Debug("Starting batch timer");
      batchTimerId = transport->TimerMicro(batchTimeoutMicro, [this]() {
        Debug("Batch timer expired with %lu items, sending",
            this->pendingBatchMessages.size());
        this->batchTimerRunning = false;

        std::function<void*()> f(std::bind(&LocalBatchSigner::asyncSignBatch, this,
          this->pendingBatchMessages, this->pendingBatchSignedMessages, this->pendingBatchCallbacks));

        this->pendingBatchMessages.clear();
        this->pendingBatchSignedMessages.clear();
        this->pendingBatchCallbacks.clear();
        this->transport->DispatchTP(f, [](void* ret){delete (bool*) ret;});
      });

    }

  }
}

//Change: main thread assembles batches. Then dispatches the batches:
// managecallback takes as arg the callback list runs those callbacks.

void* LocalBatchSigner::asyncSignBatch(std::vector<::google::protobuf::Message*> _pendingBatchMessages,
std::vector<proto::SignedMessage*> _pendingBatchSignedMessages,
std::vector<signedCallback> _pendingBatchCallbacks) {

  uint64_t batchSize = _pendingBatchMessages.size();
  {
  std::lock_guard<std::mutex> lk(stat_mutex);
    stats.IncrementList("sig_batch", batchSize);
    stats.Add("sig_batch_sizes", batchSize);
    struct timeval curr;
    gettimeofday(&curr, NULL);
    uint64_t currMicros = curr.tv_sec * 1000000ULL + curr.tv_usec;
    stats.Add("sig_batch_sizes_ts",  currMicros);
  }
  SignMessages(_pendingBatchMessages, keyManager->GetPrivateKey(id), id,
    _pendingBatchSignedMessages, merkleBranchFactor);

  for (const auto& cb : _pendingBatchCallbacks) {
    cb();
  }

  bool* ret = new bool;
  return (void*) ret;
}

//NOT USED
//in current setup, would need to take callbacks as parameter, thus making the main call the callbacks itself.
void LocalBatchSigner::ManageCallbacks(void* result){  //should take bool or the pending list as argument? Need to copy the pending list?

  // //If called by sign, then process callbacks. Otherwise this is just an empty callback to satisfy TP dispatch structure
  //   if((bool*) result){
  //     std::unique_lock<std::mutex> lk(verifierMutex);
  //
  //     for (const auto& cb : pendingBatchCallbacks) {
  //       cb();
  //     }
  //     pendingBatchCallbacks.clear();
  //     signing = false;
  //
  //     lk.unlock();
  //     cv.notify_all();
  //   }
  //
  //   delete (bool*) result;
  //   return;
}


} // namespace indicusstore
