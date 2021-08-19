/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fsp@cs.cornell.edu>
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
#include "store/benchmark/async/smallbank/bal.h"

#include "store/benchmark/async/smallbank/smallbank_transaction.h"
#include "store/benchmark/async/smallbank/utils.h"

namespace smallbank {

Bal::Bal(const std::string &cust, const uint32_t timeout)
    : SmallbankTransaction(BALANCE), cust(cust), timeout(timeout) {}

Bal::~Bal() {}

transaction_status_t Bal::Execute(SyncClient &client) {
  proto::AccountRow accountRow;
  proto::SavingRow savingRow;
  proto::CheckingRow checkingRow;
  client.Begin(timeout);
  Debug("Balance for customer %s", cust.c_str());
  if (!ReadAccountRow(client, cust, accountRow, timeout) ||
      !ReadSavingRow(client, accountRow.customer_id(), savingRow, timeout) ||
      !ReadCheckingRow(client, accountRow.customer_id(), checkingRow,
                       timeout)) {
    client.Abort(timeout);
    Debug("Aborted Balance");
    return ABORTED_USER;
  }
  transaction_status_t commitRes = client.Commit(timeout);
  Debug("Committed Balance %d",
        savingRow.saving_balance() + checkingRow.checking_balance());
  //std::pair<uint32_t, bool> res = std::make_pair(
  //    savingRow.saving_balance() + checkingRow.checking_balance(), true);
  return commitRes;
}

}  // namespace smallbank
