#include "store/benchmark/async/retwis/get_timeline.h"

#include <cstdlib>

namespace retwis {

GetTimeline::GetTimeline(Client *client, KeySelector *keySelector)
    : RetwisTransaction(client, keySelector, std::rand() % 10) {
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
