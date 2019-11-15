#include <gflags/gflags.h>

#include "lib/configuration.h"
#include "lib/latency.h"
#include "lib/tcptransport.h"
#include "store/benchmark/ping-proto.pb.h"

class PingClient : public TransportReceiver {
 public:
  PingClient(const transport::Configuration &config, Transport *transport,
      uint64_t numMessages, uint64_t messageSize) :
      config(config), transport(transport), numMessages(numMessages),
      messageSize(messageSize), receivedMessages(0UL) {
    transport->Register(this, config, -1, -1);
    for (uint64_t i = 0; i < messageSize; ++i) {
      *pm.mutable_data() += 'a' + (i % 26);
    }
    _Latency_Init(&rtt, "rtt");
  }

  virtual ~PingClient() {
    char buf[1024];
    Notice("Finished cooldown period.");
    std::sort(latencies.begin(), latencies.end());

    uint64_t ns = latencies[latencies.size()/2];
    LatencyFmtNS(ns, buf);
    Notice("Median latency is %ld ns (%s)", ns, buf);

    ns = 0;
    for (auto latency : latencies) {
      ns += latency;
    }
    ns = ns / latencies.size();
    LatencyFmtNS(ns, buf);
    Notice("Average latency is %ld ns (%s)", ns, buf);

    ns = latencies[latencies.size()*90/100];
    LatencyFmtNS(ns, buf);
    Notice("90th percentile latency is %ld ns (%s)", ns, buf);

    ns = latencies[latencies.size()*95/100];
    LatencyFmtNS(ns, buf);
    Notice("95th percentile latency is %ld ns (%s)", ns, buf);

    ns = latencies[latencies.size()*99/100];
    LatencyFmtNS(ns, buf);
    Notice("99th percentile latency is %ld ns (%s)", ns, buf);

    Latency_Dump(&rtt);
  }

  void Start() {
    Latency_Start(&rtt);
    transport->SendMessageToReplica(this, 0, pm);
  }

  virtual void ReceiveMessage(const TransportAddress &addr,
      const std::string &type, const std::string &data,
      void *meta_data) override {
    uint64_t ns = Latency_End(&rtt);
    latencies.push_back(ns);
    receivedMessages++;
    if (receivedMessages < numMessages) {
      Latency_Start(&rtt);
      transport->SendMessageToReplica(this, 0, pm);    
    } else {
      transport->Stop();
    }
  }

 private:
  const transport::Configuration &config;
  Transport *transport;
  const uint64_t numMessages;
  const uint64_t messageSize;
  uint64_t receivedMessages;
  PingMessage pm;
  Latency_t rtt;
  std::vector<uint64_t> latencies;
};

DEFINE_string(config_path, "", "path to shard configuration file");
DEFINE_uint64(num_messages, 100, "number of messages to send and receive.");
DEFINE_uint64(message_size, 1024, "number of bytes in each message.");

int main(int argc, char **argv) {
  gflags::SetUsageMessage("pings a simple ping server to measure rtt times.");
	gflags::ParseCommandLineFlags(&argc, &argv, true);

  std::ifstream configStream(FLAGS_config_path);
  if (configStream.fail()) {
    std::cerr << "Unable to read configuration file: " << FLAGS_config_path
              << std::endl;
  }

  transport::Configuration config(configStream);
  TCPTransport transport(0.0, 0.0, 0, false);
  PingClient *client = new PingClient(config, &transport, FLAGS_num_messages,
      FLAGS_message_size);
  transport.Timer(0, [client]() { client->Start(); });

  transport.Run();
  delete client;
  return 0;
}


