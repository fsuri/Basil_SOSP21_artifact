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
  uint64_t seq_num;

  proto::Transaction current_transaction;

  void SendRead(char* key);
  void HandleReadResponse(const proto::SignedReadResponse &readresponse);

  void BufferWrite(char* key, char* value);

  bool is_committing;

  void SendPrepare();
  void HandleP1Result(const proto::SignedP1Result &p1result);

  void SendP2();
  void HandleP2Echo(const proto::SignedP2Echo &p2echo);

  void SendP3();
  void HandleP3Echo(const proto::SignedP3Echo &p3echo);
};

}  // namespace bft_tapir

#endif /* _NODE_H_ */