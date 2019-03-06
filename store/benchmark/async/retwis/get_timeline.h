#ifndef RETWIS_GET_TIMELINE_H
#define RETWIS_GET_TIMELINE_H

#include <functional>

#include "store/benchmark/async/retwis/retwis_transaction.h"

namespace retwis {

class GetTimeline : public RetwisTransaction {
 public:
  GetTimeline(std::function<int()> chooseKey);
  virtual ~GetTimeline();

 protected:
  void ExecuteNextOperation(Client *client);

};

} // namespace retwis

#endif /* RETWIS_GET_TIMELINE_H */
