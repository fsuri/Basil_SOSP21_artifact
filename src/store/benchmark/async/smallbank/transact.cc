/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fs435@cornell.edu>
 *                Matthew Burke <matthelb@cs.cornell.edu>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************************/
#include "store/benchmark/async/smallbank/transact.h"
#include "store/benchmark/async/smallbank/smallbank_transaction.h"
#include "store/benchmark/async/smallbank/utils.h"

namespace smallbank {

TransactSaving::TransactSaving(const std::string &cust, const int32_t value, const uint32_t timeout) : SmallbankTransaction(TRANSACT), cust(cust), value(value), timeout(timeout) {}
TransactSaving::~TransactSaving() {
}
transaction_status_t TransactSaving::Execute(SyncClient &client) {
	proto::SavingRow savingRow;
    proto::AccountRow accountRow;

    client.Begin(timeout);
    Debug("TransactSaving for name %s with val %d", cust.c_str(), value);
    if (!ReadAccountRow(client, cust, accountRow, timeout)) {
        client.Abort(timeout);
        Debug("Aborted TransactSaving (AccountRow)");
        return ABORTED_USER;
    }
    const uint32_t customerId = accountRow.customer_id();
    if (!ReadSavingRow(client, customerId, savingRow, timeout)) {
        client.Abort(timeout);
        Debug("Aborted TransactSaving (SavingRow)");
        return ABORTED_USER;
    }
    const int32_t balance = savingRow.saving_balance();
    Debug("TransactSaving old value %d", balance);
    const int resultingBalance = balance + value;
    Debug("TransactSaving resulting %d", resultingBalance);
    if (resultingBalance < 0) {
        client.Abort(timeout);
        Debug("Aborted TransactSaving (Negative Result)");
        return ABORTED_USER;
    }
    InsertSavingRow(client, customerId, resultingBalance, timeout);
    return client.Commit(timeout);
}

} // namespace smallbank
