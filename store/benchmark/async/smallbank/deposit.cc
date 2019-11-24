#include "store/benchmark/async/smallbank/deposit.h"

#include "store/benchmark/async/smallbank/smallbank_transaction.h"
#include "store/benchmark/async/smallbank/utils.h"

namespace smallbank {

DepositChecking::DepositChecking(const std::string &cust, const int32_t value,
                                 const uint32_t timeout)
    : SmallbankTransaction(DEPOSIT),
      cust(cust),
      value(value),
      timeout(timeout) {}
      
DepositChecking::~DepositChecking() {}

int DepositChecking::Execute(SyncClient &client) {
  Debug("DepositChecking for name %s with val %d", cust.c_str(), value);
  if (value < 0) {
    client.Abort(timeout);
    Debug("Aborted DepositChecking (- val)");
    return 2;
  }
  proto::AccountRow accountRow;
  proto::CheckingRow checkingRow;

  client.Begin();
  if (!ReadAccountRow(client, cust, accountRow, timeout)) {
    client.Abort(timeout);
    Debug("Aborted DepositChecking (AccountRow)");
    return 2;
  }
  const uint32_t customerId = accountRow.customer_id();
  if (!ReadCheckingRow(client, customerId, checkingRow, timeout)) {
    client.Abort(timeout);
    Debug("Aborted DepositChecking (CheckingRow)");
    return 2;
  }
  Debug("DepositChecking old value %d", checkingRow.checking_balance());
  InsertCheckingRow(client, customerId, checkingRow.checking_balance() + value,
                    timeout);
  return client.Commit(timeout);
}

}  // namespace smallbank