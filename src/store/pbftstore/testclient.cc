#include "lib/udptransport.h"
#include "store/pbftstore/common.h"
#include "store/pbftstore/client.h"
#include "store/common/common-proto.pb.h"
#include "store/pbftstore/pbft-proto.pb.h"
#include "store/common/partitioner.h"
#include <iostream>

class NodeClient : TransportReceiver {
 public:
  NodeClient(UDPTransport *transport, const transport::Configuration &config) : transport(transport) {
    transport->Register(this, config, -1, -1);

  }
  ~NodeClient() {}

  // Message handlers.
  void ReceiveMessage(const TransportAddress &remote, const std::string &type,
                      const std::string &data, void *meta_data) {

                        std::cout << "got message" << std::endl;
                      }
  void SendTest() {
    pbftstore::proto::Request request;
    request.mutable_packed_msg()->set_msg("Hi there");
    request.mutable_packed_msg()->set_type("a type");
    request.set_digest("aaa");

    printf("sending message 1\n");
    // send to everyone and to me
    transport->SendMessageToGroup(this, 0, request);

    pbftstore::proto::Request request2;
    request2.mutable_packed_msg()->set_msg("Hi there");
    request2.mutable_packed_msg()->set_type("b msg");
    request2.set_digest("bbb");

    printf("sending message 2\n");
    // send to everyone and to me
    transport->SendMessageToGroup(this, 0, request2);

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

  KeyManager *km = new KeyManager("../../keys", crypto::ED25, true);

  int readQuorumSize = 2;
  bool signMessages = true;
  bool validateProofs = false;

  // TEST THE PBFT IMPLEMENTATION
  // NodeClient client(&transport, config);
  //
  // client.SendTest();

  // TEST THE FULL CLIENT
  // pbftstore::Client* client = new pbftstore::Client(config, 1, 1, &transport,
  //   default_partitioner, readQuorumSize, signMessages, validateProofs, km);
  DefaultPartitioner dp;
  pbftstore::Client* client = new pbftstore::Client(config, 1, 1, &transport,
    &dp, readQuorumSize, signMessages, validateProofs, km);

  auto timeoutcb = [=]() {
      printf("to\n");
    };
  auto puttimeoutcb = [=](int to, const std::string& key, const std::string& val) {
      printf("to\n");
    };
  auto gettimeoutcb= [=](int to, const std::string& key) {
      printf("to\n");
    };


  client->Begin([=](uint64_t id) {
    client->Put("A", "123", [=](int status, const std::string& key, const std::string& val) {
      client->Commit([=](transaction_status_t status) {
        printf("Committed %d\n", status);
        client->Begin([=](uint64_t id) {
          client->Get("A", [=](int status, const std::string& key, const std::string& val,
          const Timestamp& ts) {
            std::cout << "Got " << val << std::endl;
            client->Put("A", val + "fff", [=](int status, const std::string& key, const std::string& val) {
              client->Commit([=](transaction_status_t status) {
                printf("Committed2 %d\n", status);
                client->Get("A", [=](int status, const std::string& key, const std::string& val,
                const Timestamp& ts) {
                  std::cout << "Got2 " << val << std::endl;
                }, gettimeoutcb, 1000);
              }, timeoutcb, 1000);
            }, puttimeoutcb, 1000);
          }, gettimeoutcb, 1000);
        }, timeoutcb, 1000);
      }, timeoutcb, 1000);
    }, puttimeoutcb, 1000);
  }, timeoutcb, 1000);


  // TEST THE SHARD CLIENT
  // pbftstore::ShardClient* client = new pbftstore::ShardClient(config, &transport,
  // 0, signMessages, validateProofs, km);
  //
  // Timestamp ts(15, 1);
  // pbftstore::proto::Transaction txn;
  // WriteMessage* wm1 = txn.add_writeset();
  // wm1->set_key("a");
  // wm1->set_value("123");
  // ts.serialize(txn.mutable_timestamp());
  // txn.add_participating_shards(0);
  //
  // auto timeoutcb = [=](int to) {
  //     printf("to\n");
  //   };
  // auto puttimeoutcb = [=](int to, const std::string& key, const std::string& val) {
  //     printf("to\n");
  //   };
  // auto gettimeoutcb= [=](int to, const std::string& key) {
  //     printf("to\n");
  //   };
  //
  //
  // client->Begin();
  // client->Put("A", "123", [=](int status, const std::string& key, const std::string& val) {
  //   client->Commit([=](int status) {
  //     printf("Committed %d\n", status);
  //     client->Begin();
  //     client->Get("A", [=](int status, const std::string& key, const std::string& val,
  //     const Timestamp& ts) {
  //       std::cout << "Got " << val << std::endl;
  //       client->Put("A", val + "fff", [=](int status, const std::string& key, const std::string& val) {
  //         client->Commit([=](int status) {
  //           printf("Committed2 %d\n", status);
  //           client->Get("A", [=](int status, const std::string& key, const std::string& val,
  //           const Timestamp& ts) {
  //             std::cout << "Got2 " << val << std::endl;
  //           }, gettimeoutcb, 1000);
  //         }, timeoutcb, 1000);
  //       }, puttimeoutcb, 1000);
  //     }, gettimeoutcb, 1000);
  //   }, timeoutcb, 1000);
  // }, puttimeoutcb, 1000);

  // TEST THE SHARD CLIENT
  // pbftstore::ShardClient* client = new pbftstore::ShardClient(config, &transport,
  // 0, signMessages, validateProofs, km);
  //
  // Timestamp ts(15, 1);
  // pbftstore::proto::Transaction txn;
  // WriteMessage* wm1 = txn.add_writeset();
  // wm1->set_key("a");
  // wm1->set_value("123");
  // ts.serialize(txn.mutable_timestamp());
  // txn.add_participating_shards(0);
  //
  // client->SignedPrepare(txn, [=](int status, const pbftstore::proto::GroupedSignedMessage& gsd) {
  //   printf("Got decision: %d\n", status);
  //   std::string txndig = pbftstore::TransactionDigest(txn);
  //   pbftstore::proto::ShardSignedDecisions dec;
  //   (*dec.mutable_grouped_decisions())[0] = gsd;
  //   // client->CommitSigned(txndig, dec, [=]() {
  //     // printf("Wrote ack\n");
  //
  //     Timestamp ts2(17, 1);
  //     pbftstore::proto::Transaction txn2;
  //     WriteMessage* wm2 = txn2.add_writeset();
  //     wm2->set_key("a");
  //     wm2->set_value("567");
  //     ReadMessage* rm1 = txn2.add_readset();
  //     rm1->set_key("a");
  //     // set this to 14 to get a read conflict, 15 to get a valid decision
  //     Timestamp ts3(15, 1);
  //     ts3.serialize(rm1->mutable_readtime());
  //     ts2.serialize(txn2.mutable_timestamp());
  //     txn2.add_participating_shards(0);
  //     client->SignedPrepare(txn2, [=](int status, const pbftstore::proto::GroupedSignedMessage& gsd2) {
  //       printf("Got decision 2 %d\n", status);
  //       std::string txn2dig = pbftstore::TransactionDigest(txn2);
  //       pbftstore::proto::ShardSignedDecisions dec2;
  //       (*dec2.mutable_grouped_decisions())[0] = gsd2;
  //       printf("Wrote ack\n");
  //       Timestamp tsa(16, 1);
  //       pbftstore::proto::Transaction txn3;
  //       WriteMessage* wm3 = txn3.add_writeset();
  //       wm3->set_key("a");
  //       wm3->set_value("888");
  //       tsa.serialize(txn3.mutable_timestamp());
  //       txn3.add_participating_shards(0);
  //       client->SignedPrepare(txn3, [=](int status, const pbftstore::proto::GroupedSignedMessage& gsd3) {
  //         printf("Got decision 3 %d\n", status);
  //       }, [=](int to) {
  //         printf("to\n");
  //       }, 1000);
  //       // client->CommitSigned(txn2dig, dec2, [=]() {
  //
  //         // client->Get("a", tsa, 2, [=](int s, const std::string k, const std::string &v, const Timestamp& ts){
  //         //   printf("get with tsa: %d", s);
  //         //   std::cout << "get with tsa " << v << std::endl;
  //         //
  //         // }, [=](int to, const std::string& key) {
  //         //   printf("to\n");
  //         // }, 1000);
  //       // }, [=](int to) {
  //       //   printf("to\n");
  //       // }, 1000);
  //     }, [=](int to) {
  //       printf("to\n");
  //     }, 1000);
  //
  //     // Timestamp ts1(16, 1);
  //     // Timestamp ts2(14, 1);
  //     // client->Get("a", ts1, 2, [=](int s, const std::string k, const std::string &v, const Timestamp& ts){
  //     //   printf("get with ts1: %d", s);
  //     //   std::cout << "get with ts1 " << v << std::endl;
  //     //
  //     // }, [=](int to, const std::string& key) {
  //     //   printf("to\n");
  //     // }, 1000);
  //     // client->Get("a", ts2, 2, [=](int s, const std::string k, const std::string &v, const Timestamp& ts){
  //     //   printf("get with ts2: %d", s);
  //     //   std::cout << "get with ts2 " << v << std::endl;
  //     //
  //     // }, [=](int to, const std::string& key) {
  //     //   printf("to\n");
  //     // }, 1000);
  //   // }, [=](int to) {
  //   //   printf("wrote to\n");
  //   // }, 1000);
  //
  //   // printf("Key: %d", key);
  //   // printf("Value: %d", value);
  // }, [=](int to) {
  //   printf("Timeout\n");
  // }, 1000);

  transport.Run();

  return 0;
}
