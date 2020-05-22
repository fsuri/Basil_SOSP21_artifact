#include "store/indicusstore/sharedbatchverifier.h"

#include "lib/crypto.h"
#include "lib/message.h"
#include "lib/batched_sigs.h"
#include "lib/blake3.h"

#include <boost/container/container_fwd.hpp>

namespace indicusstore {

SharedBatchVerifier::SharedBatchVerifier(Stats &stats) : stats(stats) {
  segment = new managed_shared_memory(open_or_create, "signature_cache_segment",
      2 * 33554432); // 32 MB
  alloc_inst = new void_allocator(segment->get_segment_manager());
  cacheMtx = new named_sharable_mutex(open_or_create, "signature_cache_mtx");
  cache = segment->find_or_construct<SignatureCache>("signature_cache")(*alloc_inst);
}

SharedBatchVerifier::~SharedBatchVerifier() {
  // delete cacheMtx;
  // delete cache;
  // delete segment;
  // delete alloc_inst;
}

bool SharedBatchVerifier::Verify(crypto::PubKey *publicKey, const std::string &message,
    const std::string &signature) {
  MyShmString hashStrShared(BLAKE3_OUT_LEN, 0, *alloc_inst);
  MyShmString rootSigShared(crypto::SigSize(publicKey), 0, *alloc_inst);
  BatchedSigs::computeBatchedSignatureHash(&signature, &message, publicKey,
     hashStrShared, rootSigShared);

  cacheMtx->lock_sharable();
  const auto itr = cache->find(rootSigShared);
  if (itr == cache->end()) {
    cacheMtx->unlock_sharable();
    stats.Increment("verify_cache_miss");
    if (crypto::Verify(publicKey, &hashStrShared[0], hashStrShared.length(),
          &rootSigShared[0])) {
      cacheMtx->lock();
      cache->insert(std::make_pair(boost::move(rootSigShared),
           boost::move(hashStrShared)));
      cacheMtx->unlock();
      return true;
    } else {
      return false;
    }
  } else {
    bool verified = hashStrShared == itr->second;
    cacheMtx->unlock_sharable();
    if (verified) {
      stats.Increment("verify_cache_hit");
    }
    return verified;
  }
}

} // namespace indicusstore
