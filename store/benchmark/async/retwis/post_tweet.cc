#include "store/benchmark/async/retwis/post_tweet.h"

namespace retwis {

PostTweet::PostTweet(std::function<std::string()> chooseKey) :
    RetwisTransaction(chooseKey, 5) {
}

PostTweet::~PostTweet() {
}

void PostTweet::ExecuteNextOperation(Client *client) {
  std::string value;
  if (GetOpsCompleted() < 6) {
    int k = GetOpsCompleted() / 2;
    if (GetOpsCompleted() % 2 == 0) {
      client->Get(GetKey(k), value);
    } else {
      client->Put(GetKey(k), GetKey(k));
    }
  } else if (GetOpsCompleted() == 6) {
    client->Put(GetKey(3), GetKey(3));
  } else if (GetOpsCompleted() == 7) {
    client->Put(GetKey(4), GetKey(4));
  } else {
    client->Commit();
  }
}

} // namespace retwis
