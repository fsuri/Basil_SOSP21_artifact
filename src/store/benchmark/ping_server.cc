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




