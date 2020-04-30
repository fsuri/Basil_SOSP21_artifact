#include "store/benchmark/async/smallbank/amalgamate.h"

#include "store/benchmark/async/smallbank/smallbank_transaction.h"
#include "store/benchmark/async/smallbank/utils.h"

namespace smallbank {

Amalgamate::Amalgamate(const std::string &cust1, const std::string &cust2,
                       const uint32_t timeout)
    : SmallbankTransaction(AMALGAMATE),
      cust1(cust1),
      cust2(cust2),
      timeout(timeout) {}

Amalgamate::~Amalgamate() {}

transaction_status_t Amalgamate::Execute(SyncClient &client) {
  proto::AccountRow accountRow1;
  proto::AccountRow accountRow2;

  proto::CheckingRow checkingRow1;
  proto::SavingRow savingRow1;
  proto::CheckingRow checkingRow2;

  client.Begin(timeout);
  Debug("Amalgamate for names %s %s", cust1.c_str(), cust2.c_str());
  if (!ReadAccountRow(client, cust1, accountRow1, timeout) ||
      !ReadAccountRow(client, cust2, accountRow2, timeout)) {
    client.Abort(timeout);
    Debug("Aborted Amalgamate (AccountRow)");
    return ABORTED_USER;
  }
  const uint32_t customerId1 = accountRow1.customer_id();
  const uint32_t customerId2 = accountRow2.customer_id();
  if (!ReadCheckingRow(client, customerId2, checkingRow2, timeout)) {
    client.Abort(timeout);
    Debug("Aborted Amalgamate (CheckingRow)");
    return ABORTED_USER;
  }
  const int32_t balance2 = checkingRow2.checking_balance();
  if (!ReadCheckingRow(client, customerId1, checkingRow1, timeout) ||
      !ReadSavingRow(client, customerId1, savingRow1, timeout)) {
    client.Abort(timeout);
    Debug("Aborted Amalgamate (2nd Balance)");
    return ABORTED_USER;
  }
  InsertCheckingRow(
      client, customerId2,
      balance2 + checkingRow1.checking_balance() + savingRow1.saving_balance(),
      timeout);
  InsertSavingRow(client, customerId1, 0, timeout);
  InsertCheckingRow(client, customerId1, 0, timeout);
  return client.Commit(timeout);
}

}  // namespace smallbank
