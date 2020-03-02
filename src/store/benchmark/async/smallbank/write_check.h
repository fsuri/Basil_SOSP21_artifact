#ifndef WRITE_CHECK_H
#define WRITE_CHECK_H

#include "store/benchmark/async/smallbank/smallbank_transaction.h"

namespace smallbank {

class WriteCheck : public SmallbankTransaction {
 public:
  WriteCheck(const std::string &cust, const int32_t value, const uint32_t timeout);
  virtual ~WriteCheck();

  int Execute(SyncClient &client);
 private:
  std::string cust;
  int32_t value;
  uint32_t timeout;

};

} // namespace smallbank

#endif /* WRITE_CHECK_H */