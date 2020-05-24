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

  virtual bool Verify(crypto::PubKey *publicKey, const std::string &message,
      const std::string &signature) override;

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
