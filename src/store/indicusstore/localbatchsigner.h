#ifndef LOCAL_BATCH_SIGNER_H
#define LOCAL_BATCH_SIGNER_H

#include <functional>
#include <vector>
#include <atomic>

#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>

#include "lib/transport.h"
#include "lib/keymanager.h"
#include "store/indicusstore/indicus-proto.pb.h"
#include "store/indicusstore/common.h"
#include "store/common/stats.h"
#include "store/indicusstore/batchsigner.h"

#include "tbb/concurrent_vector.h"
#include "lib/concurrentqueue/concurrentqueue.h"

namespace indicusstore {

class LocalBatchSigner : public BatchSigner {
 public:
  LocalBatchSigner(Transport *transport, KeyManager *keyManager, Stats &stats,
      uint64_t batchTimeoutMicro, uint64_t batchSize, uint64_t id,
      bool adjustBatchSize, uint64_t merkleBranchFactor);
  virtual ~LocalBatchSigner();

  virtual void MessageToSign(::google::protobuf::Message* msg,
      proto::SignedMessage *signedMessage, signedCallback cb,
      bool finishBatch = false) override;

  virtual void asyncMessageToSign(::google::protobuf::Message* msg,
          proto::SignedMessage *signedMessage, signedCallback cb, bool finishBatch = false) override;

  std::mutex batchMutex;
  //Latency_t waitOnBatchLock;

 private:
  void SignBatch();
  void AdjustBatchSize();
  void* asyncSignBatch(std::vector<::google::protobuf::Message*> pendingBatchMessages,
          std::vector<proto::SignedMessage*> pendingBatchSignedMessages,
          std::vector<signedCallback> pendingBatchCallbacks);

          void* asyncSignBatch2(std::vector<Triplet> _Batch);

  void ManageCallbacks(void* result);

  std::atomic_bool batchTimerRunning;
  uint64_t batchSize;
  std::atomic_uint64_t messagesBatchedInterval;

  int batchTimerId;
  std::vector<::google::protobuf::Message*> pendingBatchMessages;
  std::vector<proto::SignedMessage*> pendingBatchSignedMessages;
  std::vector<signedCallback> pendingBatchCallbacks;

  // tbb::concurrent_vector<::google::protobuf::Message*> pendingBatchMessages;
  // tbb::concurrent_vector<proto::SignedMessage*> pendingBatchSignedMessages;
  // tbb::concurrent_vector<signedCallback> pendingBatchCallbacks;

  //tbb::concurrent_vector<Triplet> Batch;
  moodycamel::ConcurrentQueue<Triplet> Batch;

  //sync logic for multithreading
  std::mutex stat_mutex;
  std::mutex verifierMutex;
  bool signing; //true if Sign has been called. False at end of sign.
  std::condition_variable cv;


};

} // namespace indicusstore

#endif /* LOCAL_BATCH_SIGNER_H */
