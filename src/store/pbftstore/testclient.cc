#include "lib/udptransport.h"
#include "store/pbftstore/common.h"

class NodeClient : TransportReceiver {
 public:
  NodeClient(UDPTransport *transport) : transport(transport) {

  }
  ~NodeClient() {}

  // Message handlers.
  void ReceiveMessage(const TransportAddress &remote, const std::string &type,
                      const std::string &data, void *meta_data) {

                      }
  void SendTest() {
    pbftstore::proto::Request request;
    request.mutable_packed_msg()->set_msg("Hi there");
    request.mutable_packed_msg()->set_type("a type");

    // send to everyone and to me
    transport->SendMessageToReplica(this, 0, 0, request);

    transport->Timer(1000, [=]() {
      printf("Callback!!\n");
      transport->Stop();
    });
  }

private:
  Transport *transport;

};

int main(int argc, char **argv) {
  UDPTransport transport(0.0, 0.0, 0);

  NodeClient client(&transport);

  client.SendTest();

  transport.Run();

  return 0;
}
