#ifndef PING_INITIATOR_H
#define PING_INITIATOR_H

#include <map>
#include <random>

#include "lib/transport.h"

class PingInitiator {
 public:
  PingInitiator(Transport *transport);
  virtual ~PingInitiator();

 private:
  Transport *transport;
  std::random_device rd;
  std::map<uint64_t, uint64_t> outstandingSalts;
};

#endif /* PING_INITIATOR_H */
