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
#ifndef SMALLBANK_UTILS_H
#define SMALLBANK_UTILS_H

#include "store/benchmark/async/smallbank/smallbank-proto.pb.h"
#include "store/common/frontend/sync_client.h"

namespace smallbank {

    std::string AccountRowKey(const std::string name);

    std::string SavingRowKey(const uint32_t customer_id);

    std::string CheckingRowKey(const uint32_t customer_id);

    bool ReadAccountRow(SyncClient &client, const std::string &name, proto::AccountRow &accountRow, const uint32_t timeout);

	bool ReadCheckingRow(SyncClient &client, const uint32_t customer_id, proto::CheckingRow &checkingRow, const uint32_t timeout);

	bool ReadSavingRow(SyncClient &client, const uint32_t customer_id, proto::SavingRow &savingRow, const uint32_t timeout);

	void InsertAccountRow(SyncClient &client, const std::string &name, const uint32_t customer_id, const uint32_t timeout);

	void InsertSavingRow(SyncClient &client, const uint32_t customer_id, const uint32_t balance, const uint32_t timeout);

	void InsertCheckingRow(SyncClient &client, const uint32_t customer_id, const uint32_t balance, const uint32_t timeout);

} // namespace smallbank

#endif /* SMALLBANK_UTILS_H */
