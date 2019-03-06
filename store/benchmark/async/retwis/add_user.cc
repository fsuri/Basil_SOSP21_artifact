#include "store/benchmark/async/retwis/add_user.h"

namespace retwis {

AddUser::AddUser(std::function<int()> chooseKey) :
    RetwisTransaction(chooseKey, 4) {
}

AddUser::~AddUser() {
}

void AddUser::ExecuteNextOperation(Client *client) {
  if (GetOpsCompleted() == 0) {
    client->Get(GetKey(0));
  } else if (GetOpsCompleted() < 4) {
    client->Put(GetKey(GetOpsCompleted() - 1), GetKey(GetOpsCompleted() - 1));
  } else {
    client->Commit();
  }
}

} // namespace retwis
