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
  read_seq_num = 0;
  prepare_seq_num = 0;
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

  // initialize the read response tuple with the key
  max_read[read_seq_num].key = key;
  readRequest->set_clientseqnum(read_seq_num++);


  SignedReadRequest signedReadRequest;
  crypto::SignMessage(privateKey, readRequest, signedReadRequest);
  // Takes ownership of request and will make sure to delete it
  signedReadRequest.set_allocated_readrequest(readRequest);

  transport->SendMessageToAll(this, signedReadRequest);
}

bool Client::VerifyP3Commit(Transaction &transaction, P3 &p3) {
  string serialized = transaction.SerializeAsString();
  string txdigest = crypto::Hash(serialized);

  for (int i = 0; i < p3.p2echos_size(); i++) {
    SignedP2Echo signedP2Echo = p3.p2echos(i);
    P2Echo p2Echo = signedP2Echo.p2echo();
    uint64_t replicaId = p2Echo.replicaid();
    crypto::PubKey replicaPublicKey = config.getReplicaPublicKey(replicaId);
    if (crypto::IsMessageValid(replicaPublicKey, &p2Echo, &signedP2Echo) && p2Echo.txdigest() == txdigest && p2Echo.action() == COMMIT) {
      // pass
    } else {
      return false;
    }

  }
  return true;
}

bool Client::TxWritesKeyValue(Transaction &tx, string key, string value) {
  for (int i = 0; i < tx.write_size(); i++) {
    if (tx.write(i).key() == key && tx.write(i).value() == value) {
      return true;
    }
  }
  return false;
}

bool Client::VersionsEqual(const Version &v1, const Version &v2) {
  return v1.timestamp() == v2.timestamp() && v1.clientid() == v2.clientid();
}

bool Client::VersionGT(const Version &v1, const Version &v2) {
  return v1.timestamp() > v2.timestamp() || (v1.timestamp() == v2.timestamp() && v1.clientid() > v2.clientid());
}

void Client::HandleReadResponse(const SignedReadResponse &msg) {
  printf("Handling read response message\n");

  ReadResponse readResponse = msg.readresponse();
  int replicaId = readResponse.replicaid();
  uint64_t client_seq_num = readResponse.clientseqnum();
  Version version = readResponse.version();
  string key = readResponse.key();
  string value = readResponse.value();

  if (config.isValidReplicaId(replicaId)) {
    crypto::PubKey replicaPublicKey = config.getReplicaPublicKey(replicaId);

    // verify that the replica actually sent this reply and that we are expecting this reply
    if (crypto::IsMessageValid(replicaPublicKey, &readResponse, &msg) && max_read.find(client_seq_num) != max_read.end()) {
      printf("Message is valid!\n");
      cout << "Result: " << key << " -> " << value << endl;

      // Make sure that we haven't already processed this replica's reply and that the key is correct
      auto already_replied_replicas = max_read[client_seq_num].replied_replicas;
      if (already_replied_replicas.find(replicaId) != already_replied_replicas.end() && key == max_read[client_seq_num].key) {
        Transaction writeTx = readResponse.writetx();
        P3 p3 = readResponse.p3();

        // Verify the the p3 commits the given tx, that the tx version matches the read version, and that the tx writes the key
        if (VerifyP3Commit(writeTx, p3) && VersionsEqual(writeTx.version(), version) && TxWritesKeyValue(writeTx, key, value)) {
          // check if the current version is greater than the current max version
          Version max_version = max_read[client_seq_num].max_read_version;
          // This was an honest replica, so we insert it into the list of replied_replicas
          already_replied_replicas.insert(replicaId);
          if (VersionGT(version, max_version)) {
            max_read[client_seq_num].max_read_value = value;
            max_read[client_seq_num].max_read_version = version;
          }
          // TODO check if we have enough reads to return the read
        }
      }
    }
  }
}

void Client::BufferWrite(char* key, char* value) {
  Write* write = current_transaction.mutable_transaction()->add_write();
  write->set_key(key);
  write->set_value(value);
}

void Client::SendPrepare() {
  // current_transaction.mutable_transaction()->set_clientid(myId);
  current_transaction.mutable_transaction()->set_clientseqnum(prepare_seq_num++);
  struct timeval tp;
  gettimeofday(&tp, NULL);
  long int us = tp.tv_sec * 1000000 + tp.tv_usec;
  // current_transaction.mutable_transaction()->set_timestamp(us);

  crypto::SignMessage(privateKey, current_transaction.mutable_transaction(), current_transaction);

  // send prepare to all replicas
  transport->SendMessageToAll(this, current_transaction);
}

void Client::HandleP1Result(const SignedP1Result &msg) {
  printf("Handling p1 response message\n");

  P1Result p1result = msg.p1result();
  uint64_t replicaId = p1result.replicaid();
  P1Result::ConcurrencyCheckResult ccr = p1result.ccr();
  string txid = p1result.txdigest();

  if (config.isValidReplicaId(replicaId)) {
    crypto::PubKey replicaPublicKey = config.getReplicaPublicKey(replicaId);

    // verify that the replica actually sent this reply and that we are expecting this reply
    if (crypto::IsMessageValid(replicaPublicKey, &p1result, &msg)) {
      printf("Message is valid!\n");
      cout << "Result: " << txid << " -> " << ccr << endl;

      if (ccr == P1Result::COMMIT) {
        prepare_data[txid].commits.push_back(msg);
      } else if (ccr == P1Result::ABORT) {
        prepare_data[txid].aborts.push_back(msg);
      } else if (ccr == P1Result::ABSTAIN) {
        prepare_data[txid].abstains.push_back(msg);
      } else if (ccr == P1Result::RETRY) {
        prepare_data[txid].retries.push_back(msg);
      } else {
        return;
      }

      prepare_data[txid].replied_replicas.insert(replicaId);
      // TODO check p1 data to see if we can proceed to see what kind of decision to make
    }
  }
}

void Client::SendP2() {

}
void Client::HandleP2Echo(const SignedP2Echo &msg) {

}

void Client::SendP3() {

// We know that if we don't fail that the tx decision will be written
// If we do fail at this point, we know the correct tx decision will be recovered
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
