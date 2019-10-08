#ifndef SMALLBANK_TRANSACTION_H
#define SMALLBANK_TRANSACTION_H

#include "store/benchmark/async/smallbank/smallbank-proto.pb.h"
#include "store/common/frontend/sync_client.h"

namespace smallbank {
    class SmallBankTransaction {
    public:
        SmallBankTransaction(SyncClient *client, const uint32_t timeout);

//        void CreateAccount(const std::string &name, const uint32_t customer_id);

        std::pair<uint32_t, bool> Bal(const std::string &name);

        bool DepositChecking(const std::string &name, const int32_t value);

        bool TransactSaving(const std::string &name, const int32_t value);

        bool Amalgamate(const std::string &name1, const std::string &name2);

        bool WriteCheck(const std::string &name, const int32_t value);

    private:
        SyncClient *client_;

        uint32_t timeout_;

        bool ReadAccountRow(const std::string &name, proto::AccountRow &accountRow);

        bool ReadCheckingRow(const uint32_t customer_id, proto::CheckingRow &checkingRow);

        bool ReadSavingRow(const uint32_t customer_id, proto::SavingRow &savingRow);

        void InsertAccountRow(const std::string &name, const uint32_t customer_id);

        void InsertSavingRow(const uint32_t customer_id, const uint32_t balance);

        void InsertCheckingRow(const uint32_t customer_id, const uint32_t balance);
    };
}  // namespace smallbank

#endif /* SMALLBANK_TRANSACTION_H */
