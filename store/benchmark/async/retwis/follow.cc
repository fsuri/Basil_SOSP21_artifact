#include "store/benchmark/async/retwis/follow.h"

namespace retwis {

Follow::Follow(uint64_t tid, Client *client, KeySelector *keySelector) :
    RetwisTransaction(tid, client, keySelector, 2) {
}

Follow::~Follow() {

}

void Follow::ExecuteNextOperation() {
  if (GetOpsCompleted() == 0) {
    Get(GetKey(0));
  } else if (GetOpsCompleted() == 1) {
    Put(GetKey(0), GetKey(0));
  } else if (GetOpsCompleted() == 2) {
    Get(GetKey(1));
  } else if (GetOpsCompleted() == 3) {
    Put(GetKey(1), GetKey(1));
  } else {
    Commit();
  }
}

} // namespace retwis
