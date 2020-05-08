#include "store/common/pinginitiator.h"

#include "lib/message.h"

PingInitiator::PingInitiator(Transport *transport, size_t numReplicas) :
    transport(transport), numReplicas(numReplicas), alpha(0.75), length(5000) {
}

PingInitiator::~PingInitiator() {
}

void PingInitiator::Start(TransportReceiver *receiver) {
  for (size_t i = 0; i < numReplicas; ++i) {
    SendPing(receiver, i);
  }
  transport->Timer(length, [this](){
    std::set<std::pair<uint64_t, size_t>> sortedEstimates;
    for (const auto &estimate : roundTripEstimates) {
      sortedEstimates.insert(std::make_pair(estimate.second, estimate.first));
    }

    for (const auto &sortedEstimate : sortedEstimates) {
      orderedReplicas.push_back(sortedEstimate.second);
    }

    done = true;
  });
}

void PingInitiator::SendPing(TransportReceiver *receiver, size_t replica) {
  ping.set_salt(rd());
  struct timespec start;
  if (clock_gettime(CLOCK_MONOTONIC, &start) < 0) {
    PPanic("Failed to get CLOCK_MONOTONIC");
  }
  outstandingSalts.insert(std::make_pair(ping.salt(), std::make_pair(replica, start)));
  transport->SendMessageToReplica(receiver, 0, replica, ping);
}

void PingInitiator::HandlePingResponse(TransportReceiver *receiver,
    const TransportAddress &remote, const PingMessage &ping) {
  auto saltItr = outstandingSalts.find(ping.salt());
  if (saltItr != outstandingSalts.end()) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
      PPanic("Failed to get CLOCK_MONOTONIC");
    }

    uint64_t roundTrip = timespec_delta(saltItr->second.second, now);
    auto estimateItr = roundTripEstimates.find(saltItr->second.first);
    if (estimateItr == roundTripEstimates.end()) {
      roundTripEstimates.insert(std::make_pair(saltItr->second.first, roundTrip));
    } else {
      estimateItr->second = (1 - alpha) * estimateItr->second + alpha * roundTrip; 
    }

    if (!done) {
      SendPing(receiver, saltItr->second.first);
    }
    outstandingSalts.erase(saltItr);
  }
}

uint64_t PingInitiator::timespec_delta(const struct timespec &a,
      const struct timespec &b) {
  uint64_t delta;
  delta = b.tv_sec - a.tv_sec;
  delta *= 1000000000ll;
  if (b.tv_nsec < a.tv_nsec) {
    delta -= 1000000000ll;
    delta += (b.tv_nsec + 1000000000ll) - a.tv_nsec;
  } else {
    delta += b.tv_nsec - a.tv_nsec;
  }
  return delta;
}
