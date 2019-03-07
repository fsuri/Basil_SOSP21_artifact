#ifndef RETWIS_ADD_USER_H
#define RETWIS_ADD_USER_H

#include <functional>

#include "store/benchmark/async/retwis/retwis_transaction.h"

namespace retwis {

class AddUser : public RetwisTransaction {
 public:
  AddUser(std::function<std::string()> chooseKey);
  virtual ~AddUser();
 
 protected:
  void ExecuteNextOperation(Client *client);

};

} // namespace retwis

#endif /* RETWIS_ADD_USER_H */
