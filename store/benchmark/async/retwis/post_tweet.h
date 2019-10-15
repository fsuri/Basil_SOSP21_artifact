#ifndef RETWIS_POST_TWEET_H
#define RETWIS_POST_TWEET_H

#include <functional>

#include "store/benchmark/async/retwis/retwis_transaction.h"

namespace retwis {

class PostTweet : public RetwisTransaction {
 public:
  PostTweet(KeySelector *keySelector);
  virtual ~PostTweet();

 protected:
  Operation GetNextOperation(size_t opCount,
      std::map<std::string, std::string> readValues);

};

} // namespace retwis

#endif /* RETWIS_POST_TWEET_H */
