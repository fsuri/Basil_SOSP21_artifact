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

typedef std::function<void(int, const std::string &,
    const std::string &, const Timestamp &, const proto::Transaction &,
    bool)> read_callback;
typedef std::function<void(int, const std::string &)> read_timeout_callback;

typedef std::function<void(const proto::TransactionDecision&)> prepare_callback;
typedef std::function<void(int)> prepare_timeout_callback;

typedef std::function<void()> writeback_callback;
typedef std::function<void(int)> writeback_timeout_callback;

class ShardClient : public TransportReceiver {
 public:
  /* Constructor needs path to shard config. */
  ShardClient(const transport::Configuration& config, Transport *transport,
      int group_idx,
      bool signMessages, bool validateProofs,
      KeyManager *keyManager);
  ~ShardClient();

  void ReceiveMessage(const TransportAddress &remote,
      const std::string &type, const std::string &data,
      void *meta_data) override;

  // Get the value corresponding to key.
  void Get(uint64_t id, const std::string &key, const TimestampMessage &ts,
      uint64_t numResults, read_callback gcb, read_timeout_callback gtcb,
      uint32_t timeout);

  // send a request with this as the packed message
  void Prepare(const proto::Transaction& txn, prepare_callback pcb,
      prepare_timeout_callback ptcb, uint32_t timeout);

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

  // req id
  std::unordered_map<uint64_t, read_callback> pendingReads;
  std::unordered_map<std::string, prepare_callback> pendingPrepares;
  std::unordered_map<std::string, writeback_callback> pendingWritebacks;

};

} // namespace pbftstore

#endif /* _INDICUS_SHARDCLIENT_H_ */
