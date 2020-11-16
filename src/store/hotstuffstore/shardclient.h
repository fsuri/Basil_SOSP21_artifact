#ifndef _HOTSTUFF_SHARDCLIENT_H_
#define _HOTSTUFF_SHARDCLIENT_H_

#include "lib/keymanager.h"
#include "lib/assert.h"
#include "lib/configuration.h"
#include "lib/crypto.h"
#include "lib/message.h"
#include "lib/transport.h"
#include "store/common/stats.h"
#include "store/common/timestamp.h"
#include "store/common/transaction.h"
#include "store/common/common-proto.pb.h"
#include "store/hotstuffstore/pbft-proto.pb.h"
#include "store/hotstuffstore/server-proto.pb.h"

#include <map>
#include <string>

namespace hotstuffstore {

// status, key, value
typedef std::function<void(int, const std::string&, const std::string &, const Timestamp&)> read_callback;
typedef std::function<void(int, const std::string&)> read_timeout_callback;

typedef std::function<void(int, const proto::TransactionDecision&)> prepare_callback;
typedef std::function<void(int, const proto::GroupedSignedMessage&)> signed_prepare_callback;
typedef std::function<void(int)> prepare_timeout_callback;

typedef std::function<void(int)> writeback_callback;
typedef std::function<void(int)> writeback_timeout_callback;

class ShardClient : public TransportReceiver {
 public:
  /* Constructor needs path to shard config. */
  ShardClient(const transport::Configuration& config, Transport *transport,
      uint64_t group_idx,
      bool signMessages, bool validateProofs,
      KeyManager *keyManager, Stats* stats);
  ~ShardClient();

  void ReceiveMessage(const TransportAddress &remote,
      const std::string &type, const std::string &data,
      void *meta_data);

  // Get the value corresponding to key.
  void Get(const std::string &key, const Timestamp &ts,
      uint64_t numResults, read_callback gcb, read_timeout_callback gtcb,
      uint32_t timeout);

  // send a request with this as the packed message
  void Prepare(const proto::Transaction& txn, prepare_callback pcb,
      prepare_timeout_callback ptcb, uint32_t timeout);
  void SignedPrepare(const proto::Transaction& txn, signed_prepare_callback pcb,
      prepare_timeout_callback ptcb, uint32_t timeout);

  void Commit(const std::string& txn_digest, const proto::ShardDecisions& dec,
      writeback_callback wcb, writeback_timeout_callback wtcp, uint32_t timeout);
  void CommitSigned(const std::string& txn_digest, const proto::ShardSignedDecisions& dec,
      writeback_callback wcb, writeback_timeout_callback wtcp, uint32_t timeout);

  void Abort(std::string txn_digest);

 private:

  transport::Configuration config;
  Transport *transport; // Transport layer.
  int group_idx; // which shard this client accesses
  bool signMessages;
  bool validateProofs;
  KeyManager *keyManager;

  uint64_t readReq;

  struct PendingRead {
    // the set of ids that we have received a read reply for
    std::unordered_set<uint64_t> receivedReplies;
    // the max read timestamp for a valid reply
    Timestamp maxTs;
    std::string maxValue;
    proto::CommitProof maxCommitProof;

    // the current status of the reply (default to fail)
    uint64_t status;

    read_callback rcb;
    uint64_t numResultsRequired;

    Timeout* timeout;
  };

  void HandleReadReply(const proto::ReadReply& reply, const proto::SignedMessage& signedMsg);

  std::string CreateValidPackedDecision(std::string digest);

  struct PendingPrepare {
    proto::TransactionDecision validDecision;
    // if we get f+1 valid decs -> return ok
    std::unordered_set<uint64_t> receivedOkIds;
    // else, once we get f+1 failures -> return failed
    std::unordered_set<uint64_t> receivedFailedIds;
    prepare_callback pcb;

    Timeout* timeout;
  };

  struct PendingSignedPrepare {
    // the serialized packed message containing the valid transaction decision
    std::string validDecisionPacked;
    // map from id to valid signature
    std::unordered_map<uint64_t, std::string> receivedValidSigs;
    std::unordered_set<uint64_t> receivedFailedIds;
    signed_prepare_callback pcb;

    Timeout* timeout;
  };

  void HandleTransactionDecision(const proto::TransactionDecision& transactionDecision, const proto::SignedMessage& signedMsg);

  struct PendingWritebackReply {
    // set of processes we have received writeback acks from
    std::unordered_set<uint64_t> receivedAcks;
    std::unordered_set<uint64_t> receivedFails;
    writeback_callback wcb;

    Timeout* timeout;
  };

  void HandleWritebackReply(const proto::GroupedDecisionAck& groupedDecisionAck, const proto::SignedMessage& signedMsg);

  // req id to (read)
  std::unordered_map<uint64_t, PendingRead> pendingReads;
  std::unordered_map<std::string, PendingPrepare> pendingPrepares;
  std::unordered_map<std::string, PendingSignedPrepare> pendingSignedPrepares;
  std::unordered_map<std::string, PendingWritebackReply> pendingWritebacks;


  // verify that the proof asserts that the the value was written to the key
  // at the given timestamp
  bool validateReadProof(const proto::CommitProof& commitProof, const std::string& key,
    const std::string& value, const Timestamp& timestamp);


  Stats* stats;
};

} // namespace pbftstore

#endif /* _INDICUS_SHARDCLIENT_H_ */
