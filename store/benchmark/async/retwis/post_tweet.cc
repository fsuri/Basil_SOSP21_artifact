#include "store/benchmark/async/retwis/post_tweet.h"

namespace retwis {

PostTweet::PostTweet(uint64_t tid, Client *client, KeySelector *keySelector) :
    RetwisTransaction(tid, client, keySelector, 5) {
}

PostTweet::~PostTweet() {
}

void PostTweet::ExecuteNextOperation() {
  std::string value;
  if (GetOpsCompleted() < 6) {
    int k = GetOpsCompleted() / 2;
    if (GetOpsCompleted() % 2 == 0) {
      Get(GetKey(k));
    } else {
      Put(GetKey(k), GetKey(k));
    }
  } else if (GetOpsCompleted() == 6) {
    Put(GetKey(3), GetKey(3));
  } else if (GetOpsCompleted() == 7) {
    Put(GetKey(4), GetKey(4));
  } else {
    Commit();
  }
}

} // namespace retwis
