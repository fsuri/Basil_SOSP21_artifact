#include "store/benchmark/async/retwis/get_timeline.h"

#include <cstdlib>

namespace retwis {

GetTimeline::GetTimeline(uint64_t tid, KeySelector *keySelector)
    : RetwisTransaction(tid, keySelector, 1 + std::rand() % 10) {
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
