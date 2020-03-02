#ifndef BAL_H
#define BAL_H

#include "store/benchmark/async/smallbank/smallbank_transaction.h"

namespace smallbank {

class Bal : public SmallbankTransaction {
 public:
  Bal(const std::string &cust, const uint32_t timeout);

  virtual ~Bal();

  int Execute(SyncClient &client);

 private:
  std::string cust;
  uint32_t timeout;
};

}  // namespace smallbank

#endif /* BAL_H */