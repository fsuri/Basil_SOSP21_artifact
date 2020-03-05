#ifndef _NODE_CLIENT_H_
#define _NODE_CLIENT_H_

#include <memory>

#include "lib/assert.h"
#include "lib/configuration.h"
#include "lib/message.h"
#include "lib/persistent_register.h"
#include "lib/udptransport.h"
#include "lib/crypto.h"

#include "bft_tapir/messages-proto.pb.h"

#include "bft_tapir/config.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace bft_tapir {

class Client : TransportReceiver {
 public:
  Client(NodeConfig config, UDPTransport *transport, int myId);
  ~Client();

  // Message handlers.
  void ReceiveMessage(const TransportAddress &remote, const std::string &type,
                      const std::string &data, void* metadata);
  void StdinCB(char* buf);

 private:
  NodeConfig config;
  UDPTransport *transport;
  int myId;
  crypto::PrivKey privateKey;
  int currentView;

  // This is the client's message sequence number (for things like reads)
  // Should techinically be written to stable storage
  uint64_t read_seq_num;
  uint64_t prepare_seq_num;

  proto::SignedTransaction current_transaction;

  void SendRead(char* key);
  bool VerifyP3Commit(proto::Transaction &transaction, proto::P3 &p3);
  bool TxWritesKeyValue(proto::Transaction &tx, string key, string value);
  bool VersionsEqual(const proto::Version &v1, const proto::Version &v2);
  bool VersionGT(const proto::Version &v1, const proto::Version &v2);
  void HandleReadResponse(const proto::SignedReadResponse &readresponse);

  void BufferWrite(char* key, char* value);

  bool is_committing;

  void SendPrepare();
  void HandleP1Result(const proto::SignedP1Result &p1result);

  void SendP2();
  void HandleP2Echo(const proto::SignedP2Echo &p2echo);

  void SendP3();
  void HandleP3Echo(const proto::SignedP3Echo &p3echo);

  // replied replicas should hold the valid replica replies (non-byzantine replicas that replied)

  struct max_read {
    unordered_set<uint64_t> replied_replicas;
    string key;
    string max_read_value;
    proto::Version max_read_version;
  };
  // read_seq_num -> max_read
  unordered_map<uint64_t, max_read> max_read;

  struct prepare_data {
    unordered_set<uint64_t> replied_replicas;
    vector<proto::SignedP1Result> commits;
    vector<proto::SignedP1Result> aborts;
    vector<proto::SignedP1Result> abstains;
    vector<proto::SignedP1Result> retries;
  };
  // txid -> p1_data
  unordered_map<string, prepare_data> prepare_data;
  unordered_map<string, proto::TXAction> tx_decisions;

  struct p2_data {
    unordered_set<uint64_t> replied_replicas;
    // all p2 echos that echo the decision in tx_decisions
    vector<proto::SignedP2Echo> valid_echos;
  };
  // txid -> p2_data
  unordered_map<string, p2_data> p2_data;
};

}  // namespace bft_tapir

#endif /* _NODE_H_ */
