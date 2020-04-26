#ifndef DEPOSIT_H
#define DEPOSIT_H

#include "store/benchmark/async/smallbank/smallbank_transaction.h"

namespace smallbank {

class DepositChecking : public SmallbankTransaction {
 public:
  DepositChecking(const std::string &cust, const int32_t value,
                  const uint32_t timeout);

  virtual ~DepositChecking();

  transaction_status_t Execute(SyncClient &client);

 private:
  std::string cust;
  int32_t value;
  uint32_t timeout;
};

}  // namespace smallbank

#endif /* DEPOSIT_H */