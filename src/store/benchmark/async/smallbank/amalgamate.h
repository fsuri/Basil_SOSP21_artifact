#ifndef AMALGAMATE_H
#define AMALGAMATE_H

#include "store/benchmark/async/smallbank/smallbank_transaction.h"

namespace smallbank {

class Amalgamate : public SmallbankTransaction {
 public:
  Amalgamate(const std::string &cust1, const std::string &cust2,
             const uint32_t timeout);

  virtual ~Amalgamate();

  transaction_status_t Execute(SyncClient &client);

 private:
  std::string cust1;
  std::string cust2;
  uint32_t timeout;
};

}  // namespace smallbank

#endif /* AMALGAMATE_H */