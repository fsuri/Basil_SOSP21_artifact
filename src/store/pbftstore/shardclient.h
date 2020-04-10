#ifndef _PBFT_SHARDCLIENT_H_
#define _PBFT_SHARDCLIENT_H_

#include "lib/keymanager.h"
#include "lib/assert.h"
#include "lib/configuration.h"
#include "lib/crypto.h"
#include "lib/message.h"
#include "lib/transport.h"
#include "store/common/timestamp.h"
#include "store/common/transaction.h"
#include "store/common/common-proto.pb.h"
#include "store/pbftstore/pbft-proto.pb.h"
#include "store/pbftstore/server-proto.pb.h"

#include <map>
#include <string>

namespace pbftstore {

// status, key, value
typedef std::function<void(int, const std::string, const std::string &)> read_callback;
typedef std::function<void(int, const std::string &)> read_timeout_callback;

typedef std::function<void(const proto::GroupedDecisions&)> prepare_callback;
typedef std::function<void(const proto::GroupedSignedDecisions&)> signed_prepare_callback;
typedef std::function<void(int)> prepare_timeout_callback;

typedef std::function<void()> writeback_callback;
typedef std::function<void(int)> writeback_timeout_callback;

class ShardClient : public TransportReceiver {
 public:
  /* Constructor needs path to shard config. */
  ShardClient(const transport::Configuration& config, Transport *transport,
      uint64_t group_idx,
      bool signMessages, bool validateProofs,
      KeyManager *keyManager);
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
      signed_prepare_callback ptcb, uint32_t timeout);

  void Commit(const std::string& txn_digest, const proto::ShardDecisions& dec,
      writeback_callback wcb, writeback_timeout_callback wtcp, uint32_t timeout);
  void CommitSigned(const std::string& txn_digest, const proto::ShardSignedDecisions& dec,
      writeback_callback wcb, writeback_timeout_callback wtcp, uint32_t timeout);

  void Abort(std::string txn_digest, writeback_callback wcb, writeback_timeout_callback wtcp,
      uint32_t timeout);

 private:

  transport::Configuration config;
  Transport *transport; // Transport layer.
  int group_idx; // which shard this client accesses
  bool signMessages;
  bool validateProofs;
  KeyManager *keyManager;

  uint64_t readReq;

  struct PendingRead {
    std::unordered_set<uint64_t> receivedReplies;
    Timestamp maxTs;
    std::string maxValue;
    proto::CommitProof maxCommitProof;
    uint64_t status;
    read_callback rcb;
    uint64_t numResultsRequired;
  };

  struct PendingPrepare {
    // map from recv number to decision
    // must be valid for the txn
    proto::GroupedDecisions receivedDecs;
    prepare_callback pcb;
  };

  struct PendingSignedPrepare {
    std::unordered_map<uint64_t, proto::SignedMessage> receivedDecs;
    signed_prepare_callback pcb;
  };

  struct PendingWritebackReply {
    // set of processes we have received writeback acks from
    std::unordered_set<uint64_t> receivedAcks;
    writeback_callback wcb;
  };

  // req id to (read)
  std::unordered_map<uint64_t, PendingRead> pendingReads;
  std::unordered_map<std::string, PendingPrepare> pendingPrepares;
  std::unordered_map<std::string, PendingSignedPrepare> pendingSignedPrepares;
  std::unordered_map<std::string, PendingWritebackReply> pendingWritebacks;


  // verify that the proof asserts that the the value was written to the key
  // at the given timestamp
  bool validateReadProof(const proto::CommitProof& commitProof, const std::string& key,
    const std::string& value, const Timestamp& timestamp);
};

} // namespace pbftstore

#endif /* _INDICUS_SHARDCLIENT_H_ */
