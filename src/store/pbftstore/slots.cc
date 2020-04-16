#include "store/pbftstore/slots.h"
#include "store/pbftstore/common.h"
#include "lib/crypto.h"

namespace pbftstore {

Slots::Slots() {}
Slots::~Slots() {}

bool Slots::setPreprepare(const proto::Preprepare &preprepare, uint64_t replica_id, const std::string& sig) {
  uint64_t view = preprepare.viewnum();
  uint64_t seq_num = preprepare.seqnum();
  std::string digest = preprepare.digest();

  if (slots[seq_num][view].preprepare_digest.empty()) {
    slots[seq_num][view].preprepare_digest = digest;
    // add this as a prepare as well
    slots[seq_num][view].prepares[digest][replica_id] = sig;
    // TODO could do GC on all prepares and commits without the digest
    return true;
  }
  return false;
}

bool Slots::setPreprepare(const proto::Preprepare &preprepare) {
  uint64_t view = preprepare.viewnum();
  uint64_t seq_num = preprepare.seqnum();
  std::string digest = preprepare.digest();

  return setPreprepare(preprepare, slots[seq_num][view].prepares[digest].size(), "");
}

bool Slots::addPrepare(const proto::Prepare &prepare, uint64_t replica_id, const std::string& sig) {
  uint64_t view = prepare.viewnum();
  uint64_t seq_num = prepare.seqnum();
  std::string digest = prepare.digest();

  if (slots[seq_num][view].preprepare_digest.empty() || slots[seq_num][view].preprepare_digest == digest) {
    slots[seq_num][view].prepares[digest][replica_id] = sig;
    return true;
  }
  return false;
}

bool Slots::addPrepare(const proto::Prepare &prepare) {
  uint64_t view = prepare.viewnum();
  uint64_t seq_num = prepare.seqnum();
  std::string digest = prepare.digest();

  return addPrepare(prepare, slots[seq_num][view].prepares[digest].size(), "");
}

bool Slots::Prepared(uint64_t slot_num, uint64_t view, uint64_t f) {
  // first, we check if we got the preprepare message
  if (!slots[slot_num][view].preprepare_digest.empty()) {
    std::string digest = slots[slot_num][view].preprepare_digest;

    // check if we got enough prepare messages (includes the preprepare so 2f+1)
    return slots[slot_num][view].prepares[digest].size() >= 2*f + 1;
  }

  return false;
}

bool Slots::addCommit(const proto::Commit &commit, uint64_t replica_id, const std::string& sig) {
  uint64_t view = commit.viewnum();
  uint64_t seq_num = commit.seqnum();
  std::string digest = commit.digest();

  if (slots[seq_num][view].preprepare_digest.empty() || slots[seq_num][view].preprepare_digest == digest) {
    slots[seq_num][view].commits[digest][replica_id] = sig;
    return true;
  }
  return false;
}

bool Slots::addCommit(const proto::Commit &commit) {
  uint64_t view = commit.viewnum();
  uint64_t seq_num = commit.seqnum();
  std::string digest = commit.digest();

  return addCommit(commit, slots[seq_num][view].commits[digest].size(), "");
}

bool Slots::CommittedLocal(uint64_t seq_num, uint64_t view, uint64_t f) {
  if (Prepared(seq_num, view, f)) {
    // guaranteed to be valid by the Prepared predicate
    std::string digest = slots[seq_num][view].preprepare_digest;

    return slots[seq_num][view].commits[digest].size() >= 2*f + 1;
  }
  return false;
}

std::string Slots::getSlotDigest(uint64_t seq_num, uint64_t view) {
  return slots[seq_num][view].preprepare_digest;
}

}  // namespace pbftstore
