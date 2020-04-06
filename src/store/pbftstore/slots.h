#ifndef _PBFT_SLOTS_H_
#define _PBFT_SLOTS_H_

#include <cstdint>
#include <unordered_map>
#include <unordered_set>

#include "store/pbftstore/pbft-proto.pb.h"

namespace pbftstore {

class Slots {
 public:
  Slots();
  ~Slots();

  bool requestExists(const proto::Request &request);

  // All of the verified ones should have their signatures verified
  void addVerifiedRequest(const proto::Request &request);

  // return the message associated with the request digest
  proto::PackedMessage* getRequestMessage(std::string digest);

  // Primary id is the replica id that is currently the primary (the preprepare should have
  // been validated against this replica)
  // returns true if the set succeeded, if returns false -> suspect primary
  bool setVerifiedPreprepare(uint64_t primary_id, const proto::Preprepare &preprepare);

  // returns true if the set succeeded, if returns false -> suspect replica
  bool setVerifiedPrepare(const proto::Prepare &prepare, uint64_t replica_id);

  // returns true if this replica is prepared for the given view
  bool Prepared(uint64_t slot_num, uint64_t view, uint64_t f);

  // returns true if the set succeeded, if returns false -> suspect replica
  bool setVerifiedCommit(const proto::Commit &commit, uint64_t replica_id);

  // returns true if this replica is in the committed-local state for the view
  bool CommittedLocal(uint64_t slot_num, uint64_t view, uint64_t f);

  int getNumPrepared(uint64_t slot_num, uint64_t view, std::string &digest);
  int getNumCommitted(uint64_t slot_num, uint64_t view, std::string &digest);

 private:
  // digest to request
  std::unordered_map<std::string, proto::Request> requests;
  // slot number to view number to (digest,primary id) (techincally implied by the view but we don't have n here)
  std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::pair<uint64_t, std::string>>> preprepares;
  // slot number to view number to digest to set of replica ids that we got a prepare from for that digest
  // When we get a preprepare, we should add the primary to the set
  // We include the digest map because we could get prepares before we receive the preprepare (async system)
  std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::unordered_map<std::string, std::unordered_set<uint64_t>>>> prepares;
  // slot number to view number to set of replica ids that we got a commit for
  // We include the digest map because we could get commits before we receive the preprepare (async system)
  std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::unordered_map<std::string, std::unordered_set<uint64_t>>>> commits;
};

}  // namespace pbftstore

#endif
