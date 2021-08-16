/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fs435@cornell.edu>
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
#ifndef SHARED_BATCH_VERIFIER_H
#define SHARED_BATCH_VERIFIER_H

#include "store/indicusstore/verifier.h"
#include "store/common/stats.h"

#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/sync/named_sharable_mutex.hpp>

namespace indicusstore {

using namespace boost::interprocess;

class SharedBatchVerifier : public Verifier {
 public:
  SharedBatchVerifier(uint64_t merkleBranchFactor, Stats &stats);
  virtual ~SharedBatchVerifier();

  virtual bool Verify2(crypto::PubKey *publicKey, const std::string *message,
      const std::string *signature) override;
  virtual bool Verify(crypto::PubKey *publicKey, const std::string &message,
      const std::string &signature) override;

  virtual void asyncBatchVerify(crypto::PubKey *publicKey, const std::string &message,
          const std::string &signature, verifyCallback vb, bool multithread, bool autocomplete = false) override;

  virtual void Complete(bool multithread, bool force_complete = false) override;

 private:
  typedef allocator<void, managed_shared_memory::segment_manager> void_allocator;
  typedef allocator<char, managed_shared_memory::segment_manager> CharAllocator;
  typedef basic_string<char, std::char_traits<char>, CharAllocator> MyShmString;
  typedef std::pair<const MyShmString, MyShmString> MapValueType;
  typedef allocator<MapValueType, managed_shared_memory::segment_manager> MapValueTypeAllocator;
  typedef map<MyShmString, MyShmString, std::less<MyShmString>, MapValueTypeAllocator> SignatureCache;

  const uint64_t merkleBranchFactor;
  Stats &stats;

  managed_shared_memory *segment;
  const void_allocator *alloc_inst;

  named_sharable_mutex *cacheMtx;
  SignatureCache *cache;

};

} // namespace indicusstore

#endif /* SHARED_BATCH_VERIFIER_H */
