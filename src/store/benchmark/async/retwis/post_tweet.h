#ifndef RETWIS_POST_TWEET_H
#define RETWIS_POST_TWEET_H

#include <functional>

#include "store/benchmark/async/retwis/retwis_transaction.h"

namespace retwis {

class PostTweet : public RetwisTransaction {
 public:
  PostTweet(KeySelector *keySelector, std::mt19937 &rand);
  virtual ~PostTweet();

 protected:
  Operation GetNextOperation(size_t outstandingOpCount, size_t finishedOpCount,
      std::map<std::string, std::string> readValues);

};

} // namespace retwis

#endif /* RETWIS_POST_TWEET_H */
