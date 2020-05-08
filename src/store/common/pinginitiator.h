#ifndef PING_INITIATOR_H
#define PING_INITIATOR_H

#include <ctime>
#include <map>
#include <random>

#include "lib/transport.h"
#include "store/common/common-proto.pb.h"

class PingInitiator {
 public:
  PingInitiator(Transport *transport, size_t numReplicas);
  virtual ~PingInitiator();

  void Start(TransportReceiver *receiver);
  void SendPing(TransportReceiver *receiver, size_t replica);
  void HandlePingResponse(TransportReceiver *receiver, 
      const TransportAddress &remote, const PingMessage &ping);

 private:
  static uint64_t timespec_delta(const struct timespec &a,
      const struct timespec &b);
  Transport *transport;
  const size_t numReplicas;
  const double alpha;
  const uint64_t length;

  bool done;
  std::random_device rd;
  std::map<uint64_t, std::pair<size_t, struct timespec>> outstandingSalts;
  std::map<size_t, uint64_t> roundTripEstimates;

  PingMessage ping;
};

#endif /* PING_INITIATOR_H */
