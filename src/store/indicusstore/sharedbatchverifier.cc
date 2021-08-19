/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fsp@cs.cornell.edu>
 *                Matthew Burke <matthelb@cs.cornell.edu>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************************/
#include "store/indicusstore/sharedbatchverifier.h"

#include "lib/crypto.h"
#include "lib/message.h"
#include "lib/batched_sigs.h"
#include "lib/blake3.h"

#include <boost/container/container_fwd.hpp>

namespace indicusstore {

SharedBatchVerifier::SharedBatchVerifier(uint64_t merkleBranchFactor,
    Stats &stats) : merkleBranchFactor(merkleBranchFactor), stats(stats) {
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
bool SharedBatchVerifier::Verify2(crypto::PubKey *publicKey, const std::string *message,
    const std::string *signature) {
  return Verify(publicKey, *message, *signature);
}

bool SharedBatchVerifier::Verify(crypto::PubKey *publicKey, const std::string &message,
    const std::string &signature) {
  MyShmString hashStrShared(BLAKE3_OUT_LEN, 0, *alloc_inst);
  MyShmString rootSigShared(crypto::SigSize(publicKey), 0, *alloc_inst);
  if (!BatchedSigs::computeBatchedSignatureHash(&signature, &message, publicKey,
     hashStrShared, rootSigShared, merkleBranchFactor)) {
    Debug("Some hash value was tampered with.");
    return false;
  }

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
      Debug("Verification with public key failed.");
      return false;
    }
  } else {
    bool verified = hashStrShared == itr->second;
    cacheMtx->unlock_sharable();
    if (verified) {
      stats.Increment("verify_cache_hit");
    } else {
      Debug("Root hash did not match cached signature.");
    }
    return verified;
  }
}

void SharedBatchVerifier::asyncBatchVerify(crypto::PubKey *publicKey, const std::string &message,
    const std::string &signature, verifyCallback vb, bool multithread, bool autocomplete){
      return; //placeholder function to satisfy interface.
}
void SharedBatchVerifier::Complete(bool multithread, bool force_complete){
  return; //placeholder function to satisfy interface.
}

} // namespace indicusstore
