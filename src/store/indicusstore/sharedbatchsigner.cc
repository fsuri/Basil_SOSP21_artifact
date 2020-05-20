#include "store/indicusstore/sharedbatchsigner.h"

#include "lib/message.h"
#include "lib/batched_sigs.h"
#include "store/indicusstore/common.h"

namespace indicusstore {

SharedBatchSigner::SharedBatchSigner(Transport *transport,
    KeyManager *keyManager, Stats &stats, uint64_t batchTimeoutMicro,
    uint64_t batchSize, uint64_t id, bool adjustBatchSize) : BatchSigner(
      transport, keyManager, stats, batchTimeoutMicro, batchSize, id,
      adjustBatchSize), batchSize(batchSize), batchTimerId(0), nextPendingBatchId(0UL),
      alive(false) {
  segment = new managed_shared_memory(open_or_create, "MySharedMemory", 33554432);//67108864); // 64 MB
  alloc_inst = new void_allocator(segment->get_segment_manager());
  sharedWorkQueueMtx = new named_mutex(open_or_create, "shared_work_queue_mtx");
  sharedWorkQueue = segment->find_or_construct<SignatureWorkQueue>("shared_work_queue")(*alloc_inst);

  alive = true;
  signedCallbackThread = new std::thread(
      &SharedBatchSigner::RunSignedCallbackConsumer, this);
}

SharedBatchSigner::~SharedBatchSigner() {
  alive = false;
  auto cqc = GetCompletionQueueCondition(id);
  cqc->notify_one();
  signedCallbackThread->join();
  delete signedCallbackThread;
  for (const auto &mtx : completionQueueMtx) {
    delete mtx.second;
  }
  for (const auto &cond : completionQueueReady) {
    delete cond.second;
  }
  for (const auto &queue : completionQueues) {
    delete queue.second;
  }
  delete sharedWorkQueueMtx;
  delete sharedWorkQueue;
  delete segment;
  delete alloc_inst;
}

void SharedBatchSigner::MessageToSign(::google::protobuf::Message* msg,
    proto::SignedMessage *signedMessage, signedCallback cb, bool finishBatch) {
  if (initialBatchSize == 1) {
    Debug("Initial batch size = 1, immediately signing");
    SignMessage(msg, keyManager->GetPrivateKey(id), id,
        signedMessage);
    cb();
  } else {
    pendingBatchMtx.lock();
    Debug("Adding message to shared work queue.");
    uint64_t workId = nextPendingBatchId;
    nextPendingBatchId++;
    pendingBatch.insert(std::make_pair(workId, PendingBatchItem(workId, cb, signedMessage)));
    pendingBatchMtx.unlock();

    sharedWorkQueueMtx->lock();
    msg->SerializeToString(signedMessage->mutable_data());
    Debug("Adding SignatureWork with msg data %s.", 
        BytesToHex(signedMessage->data(), 100).c_str());

    sharedWorkQueue->push_back(SignatureWork(*alloc_inst, &signedMessage->data()[0],
          signedMessage->data().size(), id, workId));
    if (sharedWorkQueue->size() >= batchSize) {
      Debug("Batch is full, sending");
      if (batchTimerId > 0) {
        transport->CancelTimer(batchTimerId);
        batchTimerId = 0;
      }

      SignBatch(); 
      return;
    } else {
      sharedWorkQueueMtx->unlock();
      if (batchTimerId == 0) {
        Debug("Starting batch timeout.");
        batchTimerId = transport->TimerMicro(batchTimeoutMicro,
            std::bind(&SharedBatchSigner::BatchTimeout, this));
      }
    }
  }
}

void SharedBatchSigner::BatchTimeout() {
  batchTimerId = 0;
  pendingBatchMtx.lock();
  if (pendingBatch.size() == 0) {
    pendingBatchMtx.unlock();
    Debug("Not signing batch because have no outstanding messages.");
    return;
  }
  pendingBatchMtx.unlock();

  sharedWorkQueueMtx->lock();
  Debug("Batch timer expired with %lu items.", this->sharedWorkQueue->size());

  if (sharedWorkQueue->size() > 0) {
    this->SignBatch();
    return;
  }
  sharedWorkQueueMtx->unlock();
}
// must have lock on sharedWorkQueue
void SharedBatchSigner::SignBatch() {
  size_t batchSize = sharedWorkQueue->size();

  std::vector<const std::string *> batchMessages;
  crypto::PrivKey *privKey = keyManager->GetPrivateKey(id);
  std::vector<std::string> batchSignatures;

  std::vector<uint64_t> pids;
  std::vector<uint64_t> wids;
  std::set<uint64_t> pidsUnique;

  while (!sharedWorkQueue->empty()) {
      SignatureWork work = sharedWorkQueue->front();
      sharedWorkQueue->pop_front();
      batchMessages.push_back(new std::string(work.data.begin(), work.data.end()));
      pids.push_back(work.pid);
      wids.push_back(work.id);
      Debug("Signing message from process %lu in batch.", work.pid);
      pidsUnique.insert(work.pid);
  }

  sharedWorkQueueMtx->unlock();
  stats.IncrementList("sig_batch", batchSize);
  /*
  stats.Add("sig_batch_sizes", batchSize);
  struct timeval curr;
  gettimeofday(&curr, NULL);
  uint64_t currMicros = curr.tv_sec * 1000000ULL + curr.tv_usec;
  stats.Add("sig_batch_sizes_ts",  currMicros);*/

  BatchedSigs::generateBatchedSignatures(batchMessages, privKey, batchSignatures);

  for (size_t i = 0; i < batchSignatures.size(); ++i) {
    scoped_lock<named_mutex> lock(*GetCompletionQueueMutex(pids[i]));
    Debug("Adding signature %s %s to completion queue for %lu.", 
        BytesToHex(*batchMessages[i], 100).c_str(),
        BytesToHex(batchSignatures[i], 100).c_str(),
        pids[i]);
    GetCompletionQueue(pids[i])->push_back(SignatureWork(*alloc_inst, &batchSignatures[i][0], batchSignatures[i].size(), id, wids[i]));
    delete batchMessages[i];
  }

  for (auto pid : pidsUnique) {
    scoped_lock<named_mutex> lock(*GetCompletionQueueMutex(pid));
    Debug("Notfying %lu of completed signatures.", pid);
    GetCompletionQueueCondition(pid)->notify_one();
  }
}

void SharedBatchSigner::RunSignedCallbackConsumer() {
  Debug("Starting signed callback consumer thread.");
  while (alive) {
    scoped_lock<named_mutex> l(*GetCompletionQueueMutex(id));
    while (alive && GetCompletionQueue(id)->empty()) {
      Debug("Waiting for completed signatures.");
      GetCompletionQueueCondition(id)->wait(l);
    }

    if (alive) {
      Debug("Done waiting for completed signatures.");
    }

    pendingBatchMtx.lock();
    while (alive && !GetCompletionQueue(id)->empty()) {
      SignatureWork work = GetCompletionQueue(id)->front();
      GetCompletionQueue(id)->pop_front();
      Debug("Received signature computed by process %lu.", work.pid);
      
      if (pendingBatch.size() > 0) {
        auto itr =  pendingBatch.find(work.id);
        if (itr != pendingBatch.end()) {
          itr->second.signedMessage->set_process_id(work.pid);
          *itr->second.signedMessage->mutable_signature() = std::string(work.data.begin(), work.data.end());

          Debug("Starting timer cb.");
          transport->Timer(0, [cb = itr->second.cb](){ 
              Debug("Calling cb.");
              cb();
            });

          pendingBatch.erase(itr);
        } else {
          Debug("Signature is from stale run.");
        }
      } else {
        Debug("Signature is from stale run.");
      }
    }
    pendingBatchMtx.unlock();
  }
}

named_mutex *SharedBatchSigner::GetCompletionQueueMutex(uint64_t id) {
  auto itr = completionQueueMtx.find(id);
  if (itr == completionQueueMtx.end()) {
    auto p = completionQueueMtx.insert(std::make_pair(id, new named_mutex(open_or_create,
            ("completion_queue_mtx_" + std::to_string(id)).c_str())));
    itr = p.first;
  }
  return itr->second;
}

named_condition *SharedBatchSigner::GetCompletionQueueCondition(uint64_t id) {
  auto itr = completionQueueReady.find(id);
  if (itr == completionQueueReady.end()) {
    auto p = completionQueueReady.insert(std::make_pair(id, new named_condition(open_or_create,
            ("completion_queue_condition_" + std::to_string(id)).c_str())));
    itr = p.first;
  }
  return itr->second;
}

SharedBatchSigner::SignatureWorkQueue *SharedBatchSigner::GetCompletionQueue(uint64_t id) {
  auto itr = completionQueues.find(id);
  if (itr == completionQueues.end()) {
    auto deq = segment->find_or_construct<SignatureWorkQueue>(
        ("completion_queue_" + std::to_string(id)).c_str())(*alloc_inst);
    auto p = completionQueues.insert(std::make_pair(id, deq));
    itr = p.first;
  }
  return itr->second;
}

} // namespace indicusstore
