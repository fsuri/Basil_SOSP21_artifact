#ifndef RETWIS_POST_TWEET_H
#define RETWIS_POST_TWEET_H

#include <functional>

#include "store/benchmark/async/retwis/retwis_transaction.h"

namespace retwis {

class PostTweet : public RetwisTransaction {
 public:
  PostTweet(uint64_t tid, Client *client, KeySelector *keySelector);
  virtual ~PostTweet();

 protected:
  void ExecuteNextOperation();

};

} // namespace retwis

#endif /* RETWIS_POST_TWEET_H */
