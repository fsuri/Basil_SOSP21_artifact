#ifndef TRANSACT_H
#define TRANSACT_H

#include "store/benchmark/async/smallbank/smallbank_transaction.h"

namespace smallbank {

class TransactSaving : public SmallbankTransaction {
 public:
  TransactSaving(const std::string &cust, int32_t value, const uint32_t timeout);
  virtual ~TransactSaving();

  int Execute(SyncClient &client);
 private:
  std::string cust;
  int32_t value;
  uint32_t timeout;

};

} // namespace smallbank

#endif /* BAL_H */