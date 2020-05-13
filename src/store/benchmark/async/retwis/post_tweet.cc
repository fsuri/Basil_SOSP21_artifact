#include "store/benchmark/async/retwis/post_tweet.h"

namespace retwis {

PostTweet::PostTweet(KeySelector *keySelector, std::mt19937 &rand) :
    RetwisTransaction(keySelector, 5, rand) {
}

PostTweet::~PostTweet() {
}

Operation PostTweet::GetNextOperation(size_t outstandingOpCount, size_t finishedOpCount,
      std::map<std::string, std::string> readValues) {
  Debug("POST_TWEET %lu %lu", outstandingOpCount, finishedOpCount);
  if (outstandingOpCount < 6) {
    int k = outstandingOpCount / 2;
    if (outstandingOpCount % 2 == 0) {
      return Get(GetKey(k));
    } else {
      return Put(GetKey(k), GetKey(k));
    }
  } else if (outstandingOpCount == 6) {
    return Put(GetKey(3), GetKey(3));
  } else if (outstandingOpCount == 7) {
    return Put(GetKey(4), GetKey(4));
  } else if (outstandingOpCount == finishedOpCount) {
    return Commit();
  } else {
    return Wait();
  }
}

} // namespace retwis
