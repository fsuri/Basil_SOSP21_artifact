#include "store/benchmark/async/retwis/add_user.h"

namespace retwis {

AddUser::AddUser(KeySelector *keySelector, std::mt19937 &rand) :
    RetwisTransaction(keySelector, 4, rand) {
}

AddUser::~AddUser() {
}

Operation AddUser::GetNextOperation(size_t outstandingOpCount, size_t finishedOpCount,
      std::map<std::string, std::string> readValues) {
  Debug("ADD_USER %lu %lu", outstandingOpCount, finishedOpCount);
  if (outstandingOpCount == 0) {
    return Get(GetKey(0));
  } else if (outstandingOpCount < 4) {
    return Put(GetKey(outstandingOpCount - 1), GetKey(outstandingOpCount - 1));
  } else if(outstandingOpCount == finishedOpCount) {
    return Commit();
  } else {
    return Wait();
  }
}

} // namespace retwis
