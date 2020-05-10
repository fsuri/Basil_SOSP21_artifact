#ifndef PING_INITIATOR_H
#define PING_INITIATOR_H

#include <ctime>
#include <map>
#include <random>

#include "lib/transport.h"
#include "store/common/common-proto.pb.h"

class PingInitiator {
 public:
  PingInitiator(Transport *transport, int group, size_t numReplicas);
  virtual ~PingInitiator();

  void StartPings(TransportReceiver *receiver);

 protected:
  inline const std::vector<size_t> &GetOrderedReplicas() const { return orderedReplicas; }
  
  void HandlePingResponse(TransportReceiver *receiver, 
      const TransportAddress &remote, const PingMessage &ping);

 private:
  void SendPing(TransportReceiver *receiver, size_t replica);

  static uint64_t timespec_delta(const struct timespec &a,
      const struct timespec &b);

  Transport *transport;
  const int group;
  const size_t numReplicas;
  const double alpha;
  const uint64_t length;

  bool done;
  std::random_device rd;
  std::map<uint64_t, std::pair<size_t, struct timespec>> outstandingSalts;
  std::map<size_t, uint64_t> roundTripEstimates;

  std::vector<size_t> orderedReplicas;


  PingMessage ping;
};

#endif /* PING_INITIATOR_H */
