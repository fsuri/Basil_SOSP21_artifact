#include "store/benchmark/async/retwis/get_timeline.h"

#include <cstdlib>

namespace retwis {

GetTimeline::GetTimeline(std::function<std::string()> chooseKey)
    : RetwisTransaction(chooseKey, std::rand() % 10) {
}

GetTimeline::~GetTimeline() {
}

void GetTimeline::ExecuteNextOperation(Client *client) {
  std::string value;
  if (GetOpsCompleted() < GetNumKeys()) {
    client->Get(GetKey(GetOpsCompleted()), value);
  } else {
    client->Commit();
  }
}

} // namespace retwis
