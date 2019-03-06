#include "store/benchmark/async/retwis/follow.h"

namespace retwis {

Follow::Follow(std::function<int()> chooseKey) :
    RetwisTransaction(chooseKey, 2) {
}

Follow::~Follow() {

}

void Follow::ExecuteNextOperation(Client *client) {
  if (GetOpsCompleted() == 0) {
    client->Get(GetKey(0));
  } else if (GetOpsCompleted() == 1) {
    client->Put(GetKey(0), GetKey(0));
  } else if (GetOpsCompleted() == 2) {
    client->Get(GetKey(1));
  } else if (GetOpsCompleted() == 3) {
    client->Put(GetKey(1), GetKey(1));
  } else {
    client->Commit();
  }
}

} // namespace retwis
