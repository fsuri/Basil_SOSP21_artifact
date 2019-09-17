#ifndef SMALLBANK_CLIENT_H
#define SMALLBANK_CLIENT_H

#include "store/benchmark/async/smallbank/smallbank-proto.pb.h"
#include "store/common/frontend/sync_client.h"

namespace smallbank {
    class Benchmark {
    public:
        Benchmark(SyncClient *client);

        void CreateAccount(const std::string &name, const uint32_t &customer_id);

        uint32_t Bal(const std::string &name);

        void DepositChecking(const std::string &name, const int32_t &value);

        void TransactSaving(const std::string &name, const int32_t &value);

        void Amalgamate(const std::string &name1, const std::string &name2);

        void WriteCheck(const std::string &name, const int32_t &value);

    private:
        SyncClient *client_;

        void ReadAccountRow(const std::string &name, AccountRow &accountRow);

        void ReadCheckingRow(const uint32_t &customer_id, CheckingRow &checkingRow);

        void ReadSavingRow(const uint32_t &customer_id, SavingRow &savingRow);

        void InsertAccountRow(const std::string &name, const uint32_t &customer_id);

        void InsertSavingRow(const uint32_t &customer_id, const uint32_t &balance);

        void InsertCheckingRow(const uint32_t &customer_id, const uint32_t &balance);
    };
}  // namespace smallbank

#endif /* SMALLBANK_CLIENT_H */
