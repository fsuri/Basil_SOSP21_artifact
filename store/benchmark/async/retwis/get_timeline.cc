#include "store/benchmark/async/retwis/get_timeline.h"

#include <cstdlib>

namespace retwis {

GetTimeline::GetTimeline(uint64_t tid, Client *client, KeySelector *keySelector)
    : RetwisTransaction(tid, client, keySelector, 1 + std::rand() % 10) {
}

GetTimeline::~GetTimeline() {
}

void GetTimeline::ExecuteNextOperation() {
  std::string value;
  if (GetOpsCompleted() < GetNumKeys()) {
    Get(GetKey(GetOpsCompleted()));
  } else {
    Commit();
  }
}

} // namespace retwis
