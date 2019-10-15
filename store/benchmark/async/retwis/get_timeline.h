#ifndef RETWIS_GET_TIMELINE_H
#define RETWIS_GET_TIMELINE_H

#include <functional>

#include "store/benchmark/async/retwis/retwis_transaction.h"

namespace retwis {

class GetTimeline : public RetwisTransaction {
 public:
  GetTimeline(KeySelector *keySelector);
  virtual ~GetTimeline();

 protected:
  Operation GetNextOperation(size_t opCount,
      std::map<std::string, std::string> readValues);

};

} // namespace retwis

#endif /* RETWIS_GET_TIMELINE_H */
