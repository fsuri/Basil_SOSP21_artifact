#include "bft_tapir/replica.h"
#include "bft_tapir/cli.h"

namespace bft_tapir {

using namespace std;
using namespace proto;

Replica::Replica(NodeConfig config, int myId, Transport *transport)
    : config(config), myId(myId), transport(transport) {
  transport->Register(this, config.getReplicaConfig(), 0, myId);

  // intial view
  view = 0;
  // initial seqnum
  seqnum = 0;

  privateKey = config.getReplicaPrivateKey(myId);
}

Replica::~Replica() {}

void Replica::ReceiveMessage(const TransportAddress &remote, const string &type,
                          const string &data, void* metadata) {
  printf("Received a message\n");

  if (type == "") {
    cout << "empty type" << endl;
  } else {
    Panic("Received unexpected message type in IR proto: %s", type.c_str());
  }
}

}  // namespace bft_tapir

int main(int argc, char **argv) {
  std::pair<bft_tapir::NodeConfig, int> parsed = bft_tapir::parseCLI(argc, argv);
  bft_tapir::NodeConfig config = parsed.first;
  int myId = parsed.second;

  UDPTransport transport(0.0, 0.0, 0);

  bft_tapir::Replica replica(config, myId, &transport);

  transport.Run();

  return 0;
}