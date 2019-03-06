#include "store/benchmark/async/retwis/add_user.h"

#include <cstdlib>

namespace retwis {

GetTimeline::GetTimeline(std::function<int()> chooseKey)
    : RetwisTransaction(chooseKey, std::rand() % 10) {
}

GetTimeline::~GetTimeline() {
}

void GetTimeline::ExecuteNextOperation(Client *client) {
  if (GetOpsCompleted() < GetNumKeys()) {
    client->Get(GetKey(GetOpsCompleted());
  } else {
    client->Commit();
  }
}

} // namespace retwis
