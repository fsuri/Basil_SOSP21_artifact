#ifndef RETWIS_ADD_USER_H
#define RETWIS_ADD_USER_H

#include <functional>

#include "store/benchmark/async/retwis/retwis_transaction.h"

namespace retwis {

class AddUser : public RetwisTransaction {
 public:
  AddUser(uint64_t tid, KeySelector *keySelector);
  virtual ~AddUser();
 
 protected:
  Operation GetNextOperation(size_t opCount,
      std::map<std::string, std::string> readValues);

};

} // namespace retwis

#endif /* RETWIS_ADD_USER_H */
