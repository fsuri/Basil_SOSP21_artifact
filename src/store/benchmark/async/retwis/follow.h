#ifndef RETWIS_FOLLOW_H
#define RETWIS_FOLLOW_H

#include <functional>

#include "store/benchmark/async/retwis/retwis_transaction.h"

namespace retwis {

class Follow : public RetwisTransaction {
 public:
  Follow(KeySelector *keySelector, std::mt19937 &rand);
  virtual ~Follow();

 protected:
  Operation GetNextOperation(size_t outstandingOpCount, size_t finishedOpCount,
      std::map<std::string, std::string> readValues);

};

} // namespace retwis

#endif /* RETWIS_FOLLOW_H */
