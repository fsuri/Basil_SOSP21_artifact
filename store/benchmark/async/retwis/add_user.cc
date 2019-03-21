#include "store/benchmark/async/retwis/add_user.h"

namespace retwis {

AddUser::AddUser(uint64_t tid, Client *client, KeySelector *keySelector) :
    RetwisTransaction(tid, client, keySelector, 4) {
}

AddUser::~AddUser() {
}

void AddUser::ExecuteNextOperation() {
  if (GetOpsCompleted() == 0) {
    Get(GetKey(0));
  } else if (GetOpsCompleted() < 4) {
    Put(GetKey(GetOpsCompleted() - 1), GetKey(GetOpsCompleted() - 1));
  } else {
    Commit();
  }
}

} // namespace retwis
