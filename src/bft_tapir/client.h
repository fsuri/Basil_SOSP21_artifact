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
#include <tuple>

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
  bool TxWritesKey(proto::Transaction &tx, string key);
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

  // read_seq_num -> (max_read_key, max_read_value, max_read_version, replied_replicas)
  unordered_map<uint64_t, tuple<string, string, proto::Version, unordered_set<uint64_t>>> max_read_responses;
};

}  // namespace bft_tapir

#endif /* _NODE_H_ */
