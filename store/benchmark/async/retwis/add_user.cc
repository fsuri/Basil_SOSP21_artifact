#include "store/benchmark/async/retwis/add_user.h"

namespace retwis {

AddUser::AddUser(KeySelector *keySelector) :
    RetwisTransaction(keySelector, 4) {
}

AddUser::~AddUser() {
}

Operation AddUser::GetNextOperation(size_t opCount,
      std::map<std::string, std::string> readValues) {
  if (opCount == 0) {
    return Get(GetKey(0));
  } else if (opCount < 4) {
    return Put(GetKey(opCount - 1), GetKey(opCount - 1));
  } else {
    return Commit();
  }
}

} // namespace retwis
