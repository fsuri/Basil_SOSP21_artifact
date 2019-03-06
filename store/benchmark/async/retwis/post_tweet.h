#ifndef RETWIS_POST_TWEET_H
#define RETWIS_POST_TWEET_H

#include <functional>

#include "store/benchmark/async/retwis/retwis_transaction.h"

namespace retwis {

class PostTweet : public RetwisTransaction {
 public:
  PostTweet(std::function<int()> chooseKey);
  virtual ~PostTweet();

 protected:
  void ExecuteNextOperation(Client *client);

};

} // namespace retwis

#endif /* RETWIS_POST_TWEET_H */
