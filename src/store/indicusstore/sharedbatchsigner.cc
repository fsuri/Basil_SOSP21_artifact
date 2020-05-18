#include "store/indicusstore/sharedbatchsigner.h"

#include "lib/message.h"
#include "lib/batched_sigs.h"
#include "store/indicusstore/common.h"

namespace indicusstore {

SharedBatchSigner::SharedBatchSigner(Transport *transport,
    KeyManager *keyManager, Stats &stats, uint64_t batchTimeoutMicro,
    uint64_t batchSize, uint64_t id, bool adjustBatchSize) : BatchSigner(
      transport, keyManager, stats, batchTimeoutMicro, batchSize, id,
      adjustBatchSize), batchSize(batchSize), batchTimerId(0),
      alive(false) {
  boost::interprocess::named_mutex::remove(("completion_queue_mtx_" + std::to_string(id)).c_str());
  boost::interprocess::named_condition::remove(("completion_queue_condition_" + std::to_string(id)).c_str());
  segment = new managed_shared_memory(open_or_create, "MySharedMemory", 65536);//67108864); // 64 MB
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
  delete cqc;
  delete GetCompletionQueueMutex(id);
  delete GetCompletionQueueCondition(id);

  segment->destroy<SignatureWorkQueue>(
      ("completion_queue_" + std::to_string(id)).c_str());
  delete segment;
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
    pendingBatchSignedMessages.push(signedMessage);
    pendingBatchCallbacks.push(cb);
    pendingBatchMtx.unlock();

    sharedWorkQueueMtx->lock();
    std::string msgData(msg->SerializeAsString());
    *signedMessage->mutable_data() = msgData;
    sharedWorkQueue->push_back(SignatureWork(*alloc_inst, &msgData[0], msgData.size(), id));
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
  if (pendingBatchSignedMessages.size() == 0) {
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
  std::set<uint64_t> pidsUnique;

  while (!sharedWorkQueue->empty()) {
      SignatureWork work = sharedWorkQueue->front();
      sharedWorkQueue->pop_front();
      batchMessages.push_back(new std::string(work.data.begin(), work.data.end()));
      pids.push_back(work.pid);
      Debug("Signing message from process %lu in batch.", work.pid);
      pidsUnique.insert(work.pid);
  }

  sharedWorkQueueMtx->unlock();
  stats.IncrementList("sig_batch", batchSize);

  BatchedSigs::generateBatchedSignatures(batchMessages, privKey, batchSignatures);
  for (auto msg : batchMessages) {
    delete msg;
  }

  for (size_t i = 0; i < batchSignatures.size(); ++i) {
    scoped_lock<named_mutex> lock(*GetCompletionQueueMutex(pids[i]));
    Debug("Adding signature to completion queue for %lu.", pids[i]);
    GetCompletionQueue(pids[i])->push_back(SignatureWork(*alloc_inst, &batchSignatures[i][0], batchSignatures[i].size(), id));
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
      
      if (pendingBatchSignedMessages.size() > 0) {
        UW_ASSERT(pendingBatchCallbacks.size() > 0);
        proto::SignedMessage *signedMessage = pendingBatchSignedMessages.front();
        signedCallback cb = pendingBatchCallbacks.front();
        pendingBatchSignedMessages.pop();
        pendingBatchCallbacks.pop();

        signedMessage->set_process_id(work.pid);
        *signedMessage->mutable_signature() = std::string(work.data.begin(), work.data.end());
        transport->Timer(0, [cb](){ cb(); });
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
