#include "store/benchmark/async/retwis/post_tweet.h"

namespace retwis {

PostTweet::PostTweet(KeySelector *keySelector) :
    RetwisTransaction(keySelector, 5) {
}

PostTweet::~PostTweet() {
}

Operation PostTweet::GetNextOperation(size_t opCount,
      std::map<std::string, std::string> readValues) {
  if (opCount < 6) {
    int k = opCount / 2;
    if (opCount % 2 == 0) {
      return Get(GetKey(k));
    } else {
      return Put(GetKey(k), GetKey(k));
    }
  } else if (opCount == 6) {
    return Put(GetKey(3), GetKey(3));
  } else if (opCount == 7) {
    return Put(GetKey(4), GetKey(4));
  } else {
    return Commit();
  }
}

} // namespace retwis
