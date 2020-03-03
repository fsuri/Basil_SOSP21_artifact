#include "store/benchmark/async/retwis/follow.h"

namespace retwis {

Follow::Follow(KeySelector *keySelector, std::mt19937 &rand) :
    RetwisTransaction(keySelector, 2, rand) {
}

Follow::~Follow() {

}

Operation Follow::GetNextOperation(size_t opCount,
      std::map<std::string, std::string> readValues) {
  if (opCount == 0) {
    return Get(GetKey(0));
  } else if (opCount == 1) {
    return Put(GetKey(0), GetKey(0));
  } else if (opCount == 2) {
    return Get(GetKey(1));
  } else if (opCount == 3) {
    return Put(GetKey(1), GetKey(1));
  } else {
    return Commit();
  }
}

} // namespace retwis
