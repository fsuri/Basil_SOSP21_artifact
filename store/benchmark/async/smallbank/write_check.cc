#include "store/benchmark/async/smallbank/write_check.h"
#include "store/benchmark/async/smallbank/smallbank_transaction.h"
#include "store/benchmark/async/smallbank/utils.h"

namespace smallbank {

WriteCheck::WriteCheck(const std::string &cust, const int32_t value, const uint32_t timeout) : SmallbankTransaction(WRITE_CHECK), cust(cust), value(value), timeout(timeout) {}
WriteCheck::~WriteCheck() {
}
int WriteCheck::Execute(SyncClient &client) {
    proto::AccountRow accountRow;
    proto::CheckingRow checkingRow;
    proto::SavingRow savingRow;

    client.Begin();
    Debug("WriteCheck for name %s with value %d", cust.c_str(), value);
    if (!ReadAccountRow(client, cust, accountRow, timeout)) {
        client.Abort(timeout);
        Debug("Aborted WriteCheck (AccountRow)");
        return 1;
    }
    const uint32_t customerId = accountRow.customer_id();
    if (!ReadCheckingRow(client, customerId, checkingRow, timeout) || !
            ReadSavingRow(client, customerId, savingRow, timeout)) {
        client.Abort(timeout);
        Debug("Aborted WriteCheck (Balance)");
        return 1;
    }
    const int32_t sum = checkingRow.checking_balance() + savingRow.saving_balance();
    Debug("Sum for WriteCheck %d", sum);
    if (sum < value) {
        InsertCheckingRow(client, customerId, checkingRow.checking_balance() - value - 1, timeout);
    } else {
        InsertCheckingRow(client, customerId, checkingRow.checking_balance() - value, timeout);
    }
    client.Commit(timeout);
    Debug("Committed WriteCheck");
    return 0;
}

} // namespace smallbank