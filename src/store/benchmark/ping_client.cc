/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fs435@cornell.edu>
 *                Matthew Burke <matthelb@cs.cornell.edu>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************************/
#include <random>

#include <gflags/gflags.h>

#include "lib/configuration.h"
#include "lib/latency.h"
#include "lib/tcptransport.h"
#include "store/benchmark/ping-proto.pb.h"

class PingClient : public TransportReceiver {
 public:
  PingClient(const transport::Configuration &config, Transport *transport,
      uint64_t numMessages, uint64_t messageSize, const std::string &outFile,
      uint64_t messageSizeVariance) :
      config(config), transport(transport), numMessages(numMessages),
      messageSize(messageSize), outFile(outFile),
      messageSizeVariance(messageSizeVariance),
      dist(messageSize / (1 - static_cast<double>(messageSizeVariance) / messageSize),
          1 - static_cast<double>(messageSizeVariance) / messageSize),
      gen(0), receivedMessages(0UL) {
    transport->Register(this, config, -1, -1);
    _Latency_Init(&rtt, "rtt");
  }

  virtual ~PingClient() {
    char buf[1024];
    Notice("Finished cooldown period.");

    if (outFile.length() > 0) {
      std::ofstream ofs(outFile);
      if (ofs) {
        for (size_t i = 0; i < latencies.size(); ++i) {
          ofs << latencies[i] << std::endl; 
        }
      } else {
        Panic("Failed to write latencies to file %s.", outFile.c_str());
      }
    }

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

  void GenerateMessageData(std::string *data) {
    *data = "";
    uint64_t size = dist(gen);
    Debug("Message size: %lu.", size);
    for (uint64_t i = 0; i < size; ++i) {
      *data += 'a' + (gen() % 26); 
    }

  }

  void Start() {
    GenerateMessageData(pm.mutable_data());
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
      GenerateMessageData(pm.mutable_data());
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
  const std::string outFile;
  const uint64_t messageSizeVariance;
  std::binomial_distribution<uint64_t> dist;
  std::mt19937_64 gen;
  uint64_t receivedMessages;
  PingMessage pm;
  Latency_t rtt;
  std::vector<uint64_t> latencies;
};

DEFINE_string(config_path, "", "path to shard configuration file");
DEFINE_uint64(num_messages, 100, "number of messages to send and receive.");
DEFINE_uint64(message_size, 1024, "number of bytes in each message.");
DEFINE_string(latency_path, "", "path to latency output file");
DEFINE_uint64(message_size_variance, 0, "variance of message size in bytes");

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
      FLAGS_message_size, FLAGS_latency_path, FLAGS_message_size_variance);
  transport.Timer(0, [client]() { client->Start(); });

  transport.Run();
  delete client;
  return 0;
}


