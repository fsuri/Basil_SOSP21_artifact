#include "store/benchmark/async/retwis/add_user.h"

namespace retwis {

AddUser::AddUser(std::function<std::string()> chooseKey) :
    RetwisTransaction(chooseKey, 4) {
}

AddUser::~AddUser() {
}

void AddUser::ExecuteNextOperation(Client *client) {
  std::string value;
  if (GetOpsCompleted() == 0) {
    client->Get(GetKey(0), value);
  } else if (GetOpsCompleted() < 4) {
    client->Put(GetKey(GetOpsCompleted() - 1), GetKey(GetOpsCompleted() - 1));
  } else {
    client->Commit();
  }
}

} // namespace retwis
