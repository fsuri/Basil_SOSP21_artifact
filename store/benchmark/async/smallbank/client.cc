// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 *   SmallBank benchmarking client for a distributed transactional store.
 *
 **********************************************************************/

#include "store/benchmark/async/smallbank/client.h"
#include "store/benchmark/async/smallbank/smallbank-proto.pb.h"
#include "store/benchmark/async/smallbank/utils.h"

namespace smallbank {
    Benchmark::Benchmark(SyncClient *client, const uint32_t &timeout) : client_(client), timeout_(timeout) {
    }

    void Benchmark::CreateAccount(const std::string &name, const uint32_t &customer_id) {
        client_->Begin();
        InsertAccountRow(name, customer_id);
        InsertSavingRow(customer_id, 0);
        InsertCheckingRow(customer_id, 0);
        client_->Commit(timeout_);
    }

    uint32_t Benchmark::Bal(const std::string &name) {
        proto::AccountRow accountRow;
        proto::SavingRow savingRow;
        proto::CheckingRow checkingRow;

        client_->Begin();
        ReadAccountRow(name, accountRow);
        ReadSavingRow(accountRow.customer_id(), savingRow);
        ReadCheckingRow(accountRow.customer_id(), checkingRow);
        client_->Commit(timeout_);

        return savingRow.balance() + checkingRow.balance();
    }

    bool Benchmark::DepositChecking(const std::string &name, const int32_t &value) {
        if (value < 0) {
            Warning("Aborting deposit checking for %s due to negative amount %d", name.c_str(), value);
            return false;
        }
        proto::AccountRow accountRow;
        proto::CheckingRow checkingRow;

        client_->Begin();
        if (!ReadAccountRow(name, accountRow)) {
         return false;
        }
        const uint32_t &customerId = accountRow.customer_id();
        ReadCheckingRow(customerId, checkingRow);
        InsertCheckingRow(customerId, checkingRow.balance() + value);
        client_->Commit(timeout_);
        return true;
    }

    bool Benchmark::TransactSaving(const std::string &name, const int32_t &value) {
        proto::SavingRow savingRow;
        proto::AccountRow accountRow;

        client_->Begin();
        if (!ReadAccountRow(name, accountRow)) {
            return false;
        }
        const uint32_t &customerId = accountRow.customer_id();
        ReadSavingRow(customerId, savingRow);
        const uint32_t &balance = savingRow.balance();
        const uint32_t &resultingBalance = balance + value;
        if (resultingBalance < 0) {
            Warning("Aborting transact saving for %s due to resulting negative balance %d", name.c_str(),
                    resultingBalance);
            return false;
        }
        InsertSavingRow(customerId, resultingBalance);
        client_->Commit(timeout_);
        return true;
    }

    void Benchmark::Amalgamate(const std::string &name1, const std::string &name2) {
        proto::AccountRow accountRow1;
        proto::AccountRow accountRow2;

        proto::CheckingRow checkingRow1;
        proto::SavingRow savingRow1;
        proto::CheckingRow checkingRow2;

        client_->Begin();
        ReadAccountRow(name1, accountRow1);
        ReadAccountRow(name2, accountRow2);
        const uint32_t &customerId1 = accountRow1.customer_id();
        const uint32_t &customerId2 = accountRow2.customer_id();
        ReadCheckingRow(customerId2, checkingRow2);
        const uint32_t &balance2 = checkingRow2.balance();
        ReadCheckingRow(customerId1, checkingRow1);
        ReadSavingRow(customerId1, savingRow1);
        InsertCheckingRow(customerId2, balance2 + checkingRow1.balance() + savingRow1.balance());
        InsertSavingRow(customerId1, 0);
        InsertCheckingRow(customerId1, 0);
        client_->Commit(timeout_);
    }

    void Benchmark::WriteCheck(const std::string &name, const int32_t &value) {
        proto::AccountRow accountRow;
        proto::CheckingRow checkingRow;
        proto::SavingRow savingRow;

        client_->Begin();
        ReadAccountRow(name, accountRow);
        const uint32_t &customerId = accountRow.customer_id();
        ReadCheckingRow(customerId, checkingRow);
        ReadSavingRow(customerId, savingRow);
        const uint32_t &sum = checkingRow.balance() + savingRow.balance();
        if (sum < value) {
            InsertCheckingRow(customerId, sum - value - 1);
        } else {
            InsertCheckingRow(customerId, sum - value);
        }
        client_->Commit(timeout_);
    }

    void Benchmark::InsertAccountRow(const std::string &name, const uint32_t &customer_id) {
        proto::AccountRow accountRow;
        accountRow.set_name(name);
        accountRow.set_customer_id(customer_id);
        std::string accountRowSerialized;
        accountRow.SerializeToString(&accountRowSerialized);
        std::string accountRowKey = AccountRowKey(name);
        client_->Put(accountRowKey, accountRowSerialized, timeout_);
    }

    void Benchmark::InsertSavingRow(const uint32_t &customer_id, const uint32_t &balance) {
        proto::SavingRow savingRow;
        savingRow.set_customer_id(customer_id);
        savingRow.set_balance(balance);
        std::string savingRowSerialized;
        savingRow.SerializeToString(&savingRowSerialized);
        std::string savingRowKey = SavingRowKey(customer_id);
        client_->Put(savingRowKey, savingRowSerialized, timeout_);
    }

    void Benchmark::InsertCheckingRow(const uint32_t &customer_id, const uint32_t &balance) {
        proto::CheckingRow checkingRow;
        checkingRow.set_customer_id(customer_id);
        checkingRow.set_balance(balance);
        std::string checkingRowSerialized;
        checkingRow.SerializeToString(&checkingRowSerialized);
        std::string checkingRowKey = CheckingRowKey(customer_id);
        client_->Put(checkingRowKey, checkingRowSerialized, timeout_);
    }

    bool Benchmark::ReadAccountRow(const std::string &name, proto::AccountRow &accountRow) {
        std::string accountRowKey = AccountRowKey(name);
        std::string accountRowSerialized;
        client_->Get(accountRowKey, accountRowSerialized, timeout_);
        return accountRow.ParseFromString(accountRowSerialized);
    }

    bool Benchmark::ReadSavingRow(const uint32_t &customer_id, proto::SavingRow &savingRow) {
        std::string savingRowKey = SavingRowKey(customer_id);
        std::string savingRowSerialized;
        client_->Get(savingRowKey, savingRowSerialized, timeout_);
        return savingRow.ParseFromString(savingRowSerialized);
    }

    bool Benchmark::ReadCheckingRow(const uint32_t &customer_id, proto::CheckingRow &checkingRow) {
        std::string checkingRowKey = CheckingRowKey(customer_id);
        std::string checkingRowSerialized;
        client_->Get(checkingRowKey, checkingRowSerialized, timeout_);
        return checkingRow.ParseFromString(checkingRowSerialized);
    }
}

