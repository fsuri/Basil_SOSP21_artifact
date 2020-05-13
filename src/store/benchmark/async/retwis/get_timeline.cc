#include "store/benchmark/async/retwis/get_timeline.h"

namespace retwis {

GetTimeline::GetTimeline(KeySelector *keySelector, std::mt19937 &rand)
    : RetwisTransaction(keySelector, 1 + rand() % 10, rand) {
}

GetTimeline::~GetTimeline() {
}

Operation GetTimeline::GetNextOperation(size_t outstandingOpCount, size_t finishedOpCount,
      std::map<std::string, std::string> readValues) {
  Debug("GET_TIMELINE %lu %lu", outstandingOpCount, finishedOpCount);
  std::string value;
  if (outstandingOpCount < GetNumKeys()) {
    return Get(GetKey(outstandingOpCount));
  } else if (outstandingOpCount == finishedOpCount) {
    return Commit();
  } else {
    return Wait();
  }
}

} // namespace retwis
