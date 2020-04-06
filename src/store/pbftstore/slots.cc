#include "store/pbftstore/slots.h"
#include "store/pbftstore/common.h"
#include "lib/crypto.h"

namespace pbftstore {

Slots::Slots() {}
Slots::~Slots() {}

bool Slots::requestExists(const proto::Request &request) {
  std::string digest = RequestDigest(request);

  return requests.find(digest) != requests.end();
}

void Slots::addVerifiedRequest(const proto::Request &request) {
  std::string digest = RequestDigest(request);

  requests[digest] = request;
}

proto::PackedMessage* Slots::getRequestMessage(std::string digest) {
  return requests[digest].mutable_packed_msg();
}

bool Slots::setVerifiedPreprepare(uint64_t primary_id,
                                  const proto::Preprepare &preprepare) {
  uint64_t view = preprepare.viewnum();
  uint64_t seq_num = preprepare.seqnum();
  std::string digest = preprepare.digest();

  // we don't want to overwrite a preprepare in the same view because that
  // indicates a byzantine replica
  // access should create default empty map
  if (preprepares[seq_num].find(view) == preprepares[seq_num].end()) {
    std::pair<uint64_t, std::string> id_and_digest(primary_id, digest);
    preprepares[seq_num][view] = id_and_digest;
    // insert into prepares as well
    prepares[seq_num][view][digest].insert(primary_id);
    return true;
  }
  return false;
}

void Slots::setVerifiedPrepare(const proto::Prepare &prepare, uint64_t replica_id) {
  uint64_t view = prepare.viewnum();
  uint64_t seq_num = prepare.seqnum();
  std::string digest = prepare.digest();

  prepares[seq_num][view][digest].insert(replica_id);
}

bool Slots::Prepared(uint64_t slot_num, uint64_t view, uint64_t f) {
  // first, we check if we got the preprepare message
  if (preprepares[slot_num].find(view) != preprepares[slot_num].end()) {
    std::string digest = preprepares[slot_num][view].second;

    // check if we got enough prepare messages (includes the preprepare so 2f+1)
    return prepares[slot_num][view][digest].size() >= 2*f + 1;
  }

  return false;
}

void Slots::setVerifiedCommit(const proto::Commit &commit, uint64_t replica_id) {
  uint64_t view = commit.viewnum();
  uint64_t seq_num = commit.seqnum();
  std::string digest = commit.digest();

  commits[seq_num][view][digest].insert(replica_id);
}

bool Slots::CommittedLocal(uint64_t slot_num, uint64_t view, uint64_t f) {
  if (Prepared(slot_num, view, f)) {
    // guaranteed to exist be the Prepared predicate
    std::string digest = preprepares[slot_num][view].second;

    return commits[slot_num][view][digest].size() >= 2*f + 1;
  }
  return false;
}

int Slots::getNumPrepared(uint64_t slot_num, uint64_t view, std::string &digest) {
  return prepares[slot_num][view][digest].size();
}
int Slots::getNumCommitted(uint64_t slot_num, uint64_t view, std::string &digest) {
  return commits[slot_num][view][digest].size();
}

}  // namespace pbftstore
