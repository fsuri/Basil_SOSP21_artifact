#include "store/benchmark/async/retwis/get_timeline.h"

namespace retwis {

GetTimeline::GetTimeline(KeySelector *keySelector, std::mt19937 &rand)
    : RetwisTransaction(keySelector, 1 + rand() % 10, rand) {
}

GetTimeline::~GetTimeline() {
}

Operation GetTimeline::GetNextOperation(size_t opCount,
      std::map<std::string, std::string> readValues) {
  std::string value;
  if (opCount < GetNumKeys()) {
    return Get(GetKey(opCount));
  } else {
    return Commit();
  }
}

} // namespace retwis
