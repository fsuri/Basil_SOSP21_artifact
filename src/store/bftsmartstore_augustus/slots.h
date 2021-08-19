/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fsp@cs.cornell.edu>
 *                Zheng Wang <zw494@cornell.edu>
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
#ifndef _BFTSMART_AUGUSTUS_SLOTS_H_
#define _BFTSMART_AUGUSTUS_SLOTS_H_

#include <cstdint>
#include <unordered_map>
#include <unordered_set>

#include "store/bftsmartstore_augustus/pbft-proto.pb.h"

namespace bftsmartstore_augustus {

class Slots {
 public:
  Slots();
  ~Slots();

  // Primary id is the replica id that is currently the primary (the preprepare should have
  // been validated against this replica)
  // returns true if the set succeeded, if returns false -> suspect primary
  bool setPreprepare(const proto::Preprepare &preprepare, uint64_t replica_id, const std::string& sig);
  bool setPreprepare(const proto::Preprepare &preprepare);

  // add replica_id to the set of replicas attesting to have sent the prepare
  bool addPrepare(const proto::Prepare &prepare, uint64_t replica_id, const std::string& sig);
  bool addPrepare(const proto::Prepare &prepare);

  // returns true if this replica is prepared for the given view
  bool Prepared(uint64_t seq_num, uint64_t view, uint64_t f);

  // add replica_id to the set of replicas attesting to have sent the commit
  bool addCommit(const proto::Commit &commit, uint64_t replica_id, const std::string& sig);
  bool addCommit(const proto::Commit &commit);

  // returns true if this replica is in the committed-local state for the view
  bool CommittedLocal(uint64_t seq_num, uint64_t view, uint64_t f);

  std::string getSlotDigest(uint64_t seq_num, uint64_t view);

  proto::GroupedSignedMessage getPrepareProof(uint64_t seq_num, uint64_t view, const std::string& digest);

  proto::GroupedSignedMessage getCommitProof(uint64_t seq_num, uint64_t view, const std::string& digest);

 private:

    struct digest_and_sig {
      std::string digest;
      uint64_t replica_id;
      std::string sig;
    };

    struct Slot {
      // slot number to view number to (digest,primary id) (techincally implied by the view but we don't have n here)
      digest_and_sig preprepare;
      // map from digest to replica id to signature (may be empty)
      // we keep around multiple digests because we could received prepares before the preprepare
      // and we don't know which ones to keep
      std::unordered_map<std::string, std::unordered_map<uint64_t, std::string>> prepares;
      // map from digest to replica id to signature (may be empty)
      // these commits can be used to reconstruct commit proofs for committed digests
      std::unordered_map<std::string, std::unordered_map<uint64_t, std::string>> commits;
    };

    std::unordered_map<uint64_t, std::unordered_map<uint64_t, Slot>> slots;
};

}  // namespace bftsmartstore_augustus

#endif
