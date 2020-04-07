#include "lib/udptransport.h"
#include "store/pbftstore/common.h"

class NodeClient : TransportReceiver {
 public:
  NodeClient(UDPTransport *transport, transport::Configuration &config) : transport(transport) {
    transport->Register(this, config, -1, -1);
  }
  ~NodeClient() {}

  // Message handlers.
  void ReceiveMessage(const TransportAddress &remote, const std::string &type,
                      const std::string &data, void *meta_data) {
                        printf("OMG\n");
                      }
  void SendTest() {
    pbftstore::proto::Transaction transaction;
    transaction.mutable_timestamp()->set_id(0);
    transaction.mutable_timestamp()->set_timestamp(1);
    pbftstore::proto::Request request;
    request.mutable_packed_msg()->set_type(transaction.GetTypeName());
    request.mutable_packed_msg()->set_msg(transaction.SerializeAsString());

    // send to everyone and to me
    transport->SendMessageToGroup(this, 0, request);

    transport->Timer(1000, [=]() {
      printf("Callback!!\n");
      transport->Stop();
    });
  }

private:
  Transport *transport;

};

int main(int argc, char **argv) {
  std::ifstream configStream("test.config");
  if (configStream.fail()) {
    fprintf(stderr, "unable to read configuration file: %s\n",
            "test.config");
    exit(1);
  }
  transport::Configuration config(configStream);

  UDPTransport transport(0.0, 0.0, 0);

  NodeClient client(&transport, config);

  client.SendTest();

  transport.Run();

  return 0;
}
