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
