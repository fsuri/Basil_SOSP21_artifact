#ifndef RETWIS_FOLLOW_H
#define RETWIS_FOLLOW_H

#include <functional>

#include "store/benchmark/async/retwis/retwis_transaction.h"

namespace retwis {

class Follow : public RetwisTransaction {
 public:
  Follow(std::function<std::string()> chooseKey);
  virtual ~Follow();

 protected:
  void ExecuteNextOperation(Client *client);

};

} // namespace retwis

#endif /* RETWIS_FOLLOW_H */
