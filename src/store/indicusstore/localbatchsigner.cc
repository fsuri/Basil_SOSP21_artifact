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
    pendingBatchCallbacks.push_back(std::move(cb));

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
  //std::unique_lock<std::mutex> lock(this->batchMutex);

  if (initialBatchSize == 1) {
    Debug("Initial batch size = 1, immediately signing");

    // std::function<void*()> f(std::bind(asyncSignMessage, msg, keyManager->GetPrivateKey(id),
    //   id, signedMessage));
    // transport->DispatchTP(std::move(f), [cb](void * ret){ cb();});
    auto f = [this, msg, signedMessage, cb](){
      SignMessage(msg, keyManager->GetPrivateKey(id), id, signedMessage);
      cb();
      return (void*) true;
    };
    transport->DispatchTP_noCB(std::move(f));

  } else {
    Debug("Adding to Sig batch");
    messagesBatchedInterval++;
    pendingBatchMessages.push_back(msg);
    pendingBatchSignedMessages.push_back(signedMessage);
    pendingBatchCallbacks.push_back(std::move(cb));

    if (finishBatch || pendingBatchMessages.size() >= batchSize) {
      Debug("Batch is full, sending");
      if (batchTimerRunning) {
        transport->CancelTimer(batchTimerId);
        batchTimerRunning = false;
      }

//move these too? After moving I have a clear vector so it should be fine.
      std::function<void*()> f(std::bind(&LocalBatchSigner::asyncSignBatch, this,
        pendingBatchMessages, pendingBatchSignedMessages, std::move(pendingBatchCallbacks)));

      pendingBatchMessages.clear();
      pendingBatchSignedMessages.clear();
      pendingBatchCallbacks.clear();
      Debug("Batch request bound, dispatching");
      //transport->DispatchTP(std::move(f), [](void* ret){delete (bool*) ret;});
      transport->DispatchTP_noCB(std::move(f));


    } else if (!batchTimerRunning) {
      batchTimerRunning = true;
      Debug("Starting batch timer");
      batchTimerId = transport->TimerMicro(batchTimeoutMicro, [this]() {
        //std::unique_lock<std::mutex> lock(this->batchMutex);
        Debug("Batch timer expired with %lu items, sending",
            this->pendingBatchMessages.size());
        this->batchTimerRunning = false;

        std::function<void*()> f(std::bind(&LocalBatchSigner::asyncSignBatch, this,
          this->pendingBatchMessages, this->pendingBatchSignedMessages, this->pendingBatchCallbacks));

        this->pendingBatchMessages.clear();
        this->pendingBatchSignedMessages.clear();
        this->pendingBatchCallbacks.clear();
        //this->transport->DispatchTP(std::move(f), [](void* ret){delete (bool*) ret;});
        this->transport->DispatchTP_noCB(std::move(f));

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
  Debug("(CPU:%d) Signing batch", sched_getcpu());
  SignMessages(_pendingBatchMessages, keyManager->GetPrivateKey(id), id,
    _pendingBatchSignedMessages, merkleBranchFactor);

  Debug("(CPU:%d) Issuing sender callbacks", sched_getcpu());
  for (const auto& cb : _pendingBatchCallbacks) {
    cb();
  }

  //bool* ret = new bool;
  return (void*) true;
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
