#include "lib/tcptransport.h"
#include "lib/configuration.h"
#include "store/janusstore/server.h"
#include "store/janusstore/janus-proto.pb.h"

#include <arpa/inet.h>
#include <iostream>

using namespace janusstore::proto;

int main(int argc, char **argv) {
  printf("wtf\n");
  // transport is used to send messages btwn replicas and schedule msgs
  TCPTransport transport(0.0, 0.0, 0, false);
  TCPTransport transport2(0.0, 0.0, 0, false);
  printf("hi\n");
  std::ifstream configStream ("store/janus1.config");
  transport::Configuration config(configStream);
  printf("config generated\n");
  janusstore::Server *server = new janusstore::Server(config, 0, &transport);
  janusstore::Server *server1 = new janusstore::Server(config, 1, &transport2);
  printf("servers started\n");
  Request request;
  PreAcceptMessage preaccept;
  janusstore::proto::TransactionMessage txn;
  txn.set_serverip("127.0.0.1");
  txn.set_serverport(10001);
  txn.set_txnid(0);
  txn.set_status(janusstore::proto::TransactionMessage::PREACCEPT);
  request.set_op(Request::PREACCEPT);
  request.set_allocated_preaccept(&preaccept);
  printf("running\n");
  transport::ReplicaAddress ra_addr = config.replica(1);
  // std::cout << ra_addr.host << " " << ra_addr.port << "\n";
  TCPTransportAddress addr = transport.LookupAddress(ra_addr);

  // TODO why cant server0 find server1
  std::cout << server->transport->SendMessageToReplica(server, 1, request);

  printf("ran\n");
  while (true) {}
  return 0;
}
