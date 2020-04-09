#include "store/pbftstore/shardclient.h"

namespace pbftstore {

ShardClient::ShardClient(const transport::Configuration& config, Transport *transport,
    uint64_t group_idx,
    bool signMessages, bool validateProofs,
    KeyManager *keyManager) :
    config(config), transport(transport),
    group_idx(group_idx),
    signMessages(signMessages), validateProofs(validateProofs),
    keyManager(keyManager) {
  transport->Register(this, config, -1, -1);
}

void ShardClient::ReceiveMessage(const TransportAddress &remote,
    const std::string &type, const std::string &data,
    void *meta_data) {
  proto::SignedMessage signedMessage;
  std::string type;
  std::string data;
  if (t == signedMessage.GetTypeName()) {
    if (!signedMessage.ParseFromString(d)) {
      return;
    }

    if (!ValidateSignedMessage(signedMessage, keyManager, data, type)) {
      return;
    }
  } else {
    type = t;
    data = d;
  }

  proto::ReadReply readReply;
  proto::TransactionDecision transactionDecision;
  proto::GroupedDecisionAck groupedDecisionAck;
  if ()
}

// Get the value corresponding to key.
void ShardClient::Get(uint64_t id, const std::string &key, const TimestampMessage &ts,
    uint64_t numResults, read_callback gcb, read_timeout_callback gtcb,
    uint32_t timeout) {

}

// send a request with this as the packed message
void ShardClient::Prepare(const proto::Transaction& txn, prepare_callback pcb,
    prepare_timeout_callback ptcb, uint32_t timeout) {

}

void ShardClient::Commit(const std::string& txn_digest, const proto::ShardDecisions& dec,
    writeback_callback wcb, writeback_timeout_callback wtcp, uint32_t timeout) {

}

void ShardClient::CommitSigned(const std::string& txn_digest, const proto::ShardSignedDecisions& dec,
    writeback_callback wcb, writeback_timeout_callback wtcp, uint32_t timeout) {

}

void ShardClient::Abort(std::string txn_digest, writeback_callback wcb, writeback_timeout_callback wtcp,
    uint32_t timeout) {

}

}
