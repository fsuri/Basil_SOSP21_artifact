#ifndef RETWIS_FOLLOW_H
#define RETWIS_FOLLOW_H

#include <functional>

#include "store/benchmark/async/retwis/retwis_transaction.h"

namespace retwis {

class Follow : public RetwisTransaction {
 public:
  Follow(Client *client, KeySelector *keySelector);
  virtual ~Follow();

 protected:
  void ExecuteNextOperation();

};

} // namespace retwis

#endif /* RETWIS_FOLLOW_H */
