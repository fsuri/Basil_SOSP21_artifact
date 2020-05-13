#include "store/benchmark/async/retwis/follow.h"

namespace retwis {

Follow::Follow(KeySelector *keySelector, std::mt19937 &rand) :
    RetwisTransaction(keySelector, 2, rand) {
}

Follow::~Follow() {

}

Operation Follow::GetNextOperation(size_t outstandingOpCount, size_t finishedOpCount,
      std::map<std::string, std::string> readValues) {
  Debug("FOLLOW %lu %lu", outstandingOpCount, finishedOpCount);
  if (outstandingOpCount == 0) {
    return Get(GetKey(0));
  } else if (outstandingOpCount == 1) {
    return Put(GetKey(0), GetKey(0));
  } else if (outstandingOpCount == 2) {
    return Get(GetKey(1));
  } else if (outstandingOpCount == 3) {
    return Put(GetKey(1), GetKey(1));
  } else if (outstandingOpCount == finishedOpCount) {
    return Commit();
  } else {
    return Wait();
  }
}

} // namespace retwis
