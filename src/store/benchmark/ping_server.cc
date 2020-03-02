#include <csignal>

#include <gflags/gflags.h>

#include "lib/configuration.h"
#include "lib/tcptransport.h"
#include "store/benchmark/ping-proto.pb.h"

class PingServer : public TransportReceiver {
 public:
  PingServer(const transport::Configuration &config, Transport *transport) :
      config(config), transport(transport) {
    transport->Register(this, config, 0, 0);
  }

  virtual ~PingServer() { }

  virtual void ReceiveMessage(const TransportAddress &addr,
      const std::string &type, const std::string &data,
      void *meta_data) override {
    pm.ParseFromString(data);
    transport->SendMessage(this, addr, pm);
  }

 private:
  const transport::Configuration &config;
  Transport *transport;
  PingMessage pm;

};

DEFINE_string(config_path, "", "path to shard configuration file");

void Cleanup(int signal);

PingServer *server = nullptr;

int main(int argc, char **argv) {
  gflags::SetUsageMessage("runs a simple ping server for measuring rtt times.");
	gflags::ParseCommandLineFlags(&argc, &argv, true);

  // parse configuration
  std::ifstream configStream(FLAGS_config_path);
  if (configStream.fail()) {
    std::cerr << "Unable to read configuration file: " << FLAGS_config_path
              << std::endl;
  }

  transport::Configuration config(configStream);

  TCPTransport transport(0.0, 0.0, 0);

  server = new PingServer(config, &transport);

  std::signal(SIGKILL, Cleanup);
  std::signal(SIGTERM, Cleanup);
  std::signal(SIGINT, Cleanup);
  transport.Run();
  return 0;
}

void Cleanup(int signal) {
  delete server;
  exit(0);
}




