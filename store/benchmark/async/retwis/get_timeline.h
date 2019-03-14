#ifndef RETWIS_GET_TIMELINE_H
#define RETWIS_GET_TIMELINE_H

#include <functional>

#include "store/benchmark/async/retwis/retwis_transaction.h"

namespace retwis {

class GetTimeline : public RetwisTransaction {
 public:
  GetTimeline(Client *client, KeySelector *keySelector);
  virtual ~GetTimeline();

 protected:
  void ExecuteNextOperation();

};

} // namespace retwis

#endif /* RETWIS_GET_TIMELINE_H */
