#include "bft_tapir/client.h"
#include "bft_tapir/cli.h"

namespace bft_tapir {

using namespace std;
using namespace proto;

Client::Client(NodeConfig config, UDPTransport *transport, int myId)
    : config(config), transport(transport), myId(myId) {
  transport->Register(this, config.getReplicaConfig(), 0, -1);
  privateKey = config.getClientPrivateKey(myId);
  currentView = 0;
  seq_num = 0;
  is_committing = false;
}

Client::~Client() {}

void Client::ReceiveMessage(const TransportAddress &remote, const string &type,
                            const string &data, void* metadata) {
  printf("got a message\n");
  SignedReadResponse signedReadResponse;
  SignedP1Result signedP1Result;
  SignedP2Echo signedP2Echo;
  SignedP3Echo signedP3Echo;

  if (type == signedReadResponse.GetTypeName()) {
    signedReadResponse.ParseFromString(data);
    HandleReadResponse(signedReadResponse);
  } else if (type == signedP1Result.GetTypeName()) {
    signedP1Result.ParseFromString(data);
    HandleP1Result(signedP1Result);
  } else if (type == signedP2Echo.GetTypeName()) {
    signedP2Echo.ParseFromString(data);
    HandleP2Echo(signedP2Echo);
  } else if (type == signedP3Echo.GetTypeName()) {
    signedP3Echo.ParseFromString(data);
    HandleP3Echo(signedP3Echo);
  } else {
    Panic("Received unexpected message type in IR proto: %s", type.c_str());
  }
}

// api:
// r <key> -> prints result
// w <key> <value>
// c -> attempts to commit, prints ok when done
void Client::StdinCB(char* buf) {
  if (buf[0] == 'r') {
    char* key = strtok(buf + 2, " ");
    Notice("Reading %s", key);
    SendRead(key);
  } else if (buf[0] == 'w') {
    char* key = strtok(buf + 2, " ");
    char* value = strtok(NULL, "");
    Notice("Setting %s to %s", key, value);
    BufferWrite(key, value);
  } else if (buf[0] == 'c') {
    Notice("Committing Transaction");
  } else {
    Warning("Invalid command");
  }
}

void Client::SendRead(char* key) {
  ReadRequest *readRequest = new ReadRequest();
  readRequest->set_key(key);
  readRequest->set_clientid(myId);
  readRequest->set_clientseqnum(seq_num++);

  SignedReadRequest signedReadRequest;
  crypto::SignMessage(privateKey, readRequest, signedReadRequest);
  // Takes ownership of request and will make sure to delete it
  signedReadRequest.set_allocated_readrequest(readRequest);

  transport->SendMessageToAll(this, signedReadRequest);
}

void Client::HandleReadResponse(const SignedReadResponse &readresponse) {

}

void Client::BufferWrite(char* key, char* value) {
  Write* write = current_transaction.add_write();
  write->set_key(key);
  write->set_value(value);
}

void Client::SendPrepare() {

}

void Client::HandleP1Result(const SignedP1Result &p1result) {

}

void Client::SendP2() {

}
void Client::HandleP2Echo(const SignedP2Echo &p2echo) {

}

void Client::SendP3() {

}
void Client::HandleP3Echo(const SignedP3Echo &p3echo) {

// TODO reset transaction and is_committing after 2f+1 confirmations
}

}  // namespace bft_tapir

int main(int argc, char **argv) {
  std::pair<bft_tapir::NodeConfig, int> parsed = bft_tapir::parseCLI(argc, argv);
  bft_tapir::NodeConfig config = parsed.first;
  int myId = parsed.second;

  UDPTransport transport(0.0, 0.0, 0);

  bft_tapir::Client client(config, &transport, myId);

  transport.Run();

  return 0;
}