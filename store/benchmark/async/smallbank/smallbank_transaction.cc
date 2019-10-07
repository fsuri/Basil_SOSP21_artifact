// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 *   SmallBank transactions.
 *
 **********************************************************************/

#include "store/benchmark/async/smallbank/smallbank_transaction.h"
#include "store/benchmark/async/smallbank/smallbank-proto.pb.h"
#include "store/benchmark/async/smallbank/utils.h"
#include "store/common/frontend/sync_client.h"

namespace smallbank {
    SmallBankTransaction::SmallBankTransaction(SyncClient *client, const uint32_t timeout) : client_(client), timeout_(timeout) {
    }

    void SmallBankTransaction::CreateAccount(const std::string &name, const uint32_t customer_id) {
        client_->Begin();
        InsertAccountRow(name, customer_id);
        InsertSavingRow(customer_id, 0);
        InsertCheckingRow(customer_id, 0);
        client_->Commit(timeout_);
    }

    std::pair<uint32_t, bool> SmallBankTransaction::Bal(const std::string &name) {
        proto::AccountRow accountRow;
        proto::SavingRow savingRow;
        proto::CheckingRow checkingRow;

        client_->Begin();
        if (!ReadAccountRow(name, accountRow) || !ReadSavingRow(accountRow.customer_id(), savingRow) ||
            !ReadCheckingRow(accountRow.customer_id(), checkingRow)) {
            client_->Abort(timeout_);
            return std::make_pair(0, false);
        }
        client_->Commit(timeout_);

        return std::make_pair(savingRow.saving_balance() + checkingRow.checking_balance(), true);
    }

    bool SmallBankTransaction::DepositChecking(const std::string &name, const int32_t value) {
        if (value < 0) {
            Warning("Aborting deposit checking for %s due to negative amount %d", name.c_str(), value);
            client_->Abort(timeout_);
            return false;
        }
        proto::AccountRow accountRow;
        proto::CheckingRow checkingRow;

        client_->Begin();
        if (!ReadAccountRow(name, accountRow)) {
            client_->Abort(timeout_);
            return false;
        }
        const uint32_t customerId = accountRow.customer_id();
        if (!ReadCheckingRow(customerId, checkingRow)) {
            client_->Abort(timeout_);
            return false;
        }
        InsertCheckingRow(customerId, checkingRow.checking_balance() + value);
        client_->Commit(timeout_);
        return true;
    }

    bool SmallBankTransaction::TransactSaving(const std::string &name, const int32_t value) {
        proto::SavingRow savingRow;
        proto::AccountRow accountRow;

        client_->Begin();
        if (!ReadAccountRow(name, accountRow)) {
            client_->Abort(timeout_);
            return false;
        }
        const uint32_t customerId = accountRow.customer_id();
        ReadSavingRow(customerId, savingRow);
        const uint32_t balance = savingRow.saving_balance();
        const uint32_t resultingBalance = balance + value;
        if (resultingBalance < 0) {
            client_->Abort(timeout_);
            Warning("Aborting transact saving for %s due to resulting negative balance %d", name.c_str(),
                    resultingBalance);
            return false;
        }
        InsertSavingRow(customerId, resultingBalance);
        client_->Commit(timeout_);
        return true;
    }

    bool SmallBankTransaction::Amalgamate(const std::string &name1, const std::string &name2) {
        proto::AccountRow accountRow1;
        proto::AccountRow accountRow2;

        proto::CheckingRow checkingRow1;
        proto::SavingRow savingRow1;
        proto::CheckingRow checkingRow2;

        client_->Begin();
        if (!ReadAccountRow(name1, accountRow1) || !ReadAccountRow(name2, accountRow2)) {
            client_->Abort(timeout_);
            return false;
        }
        const uint32_t customerId1 = accountRow1.customer_id();
        const uint32_t customerId2 = accountRow2.customer_id();
        if (!ReadCheckingRow(customerId2, checkingRow2)) {
            client_->Abort(timeout_);
            return false;
        }
        const uint32_t balance2 = checkingRow2.checking_balance();
        if (!ReadCheckingRow(customerId1, checkingRow1) || !
                ReadSavingRow(customerId1, savingRow1)) {
            client_->Abort(timeout_);
            return false;
        }
        InsertCheckingRow(customerId2, balance2 + checkingRow1.checking_balance() + savingRow1.saving_balance());
        InsertSavingRow(customerId1, 0);
        InsertCheckingRow(customerId1, 0);
        client_->Commit(timeout_);
        return true;
    }

    bool SmallBankTransaction::WriteCheck(const std::string &name, const int32_t value) {
        proto::AccountRow accountRow;
        proto::CheckingRow checkingRow;
        proto::SavingRow savingRow;

        client_->Begin();
        if (!ReadAccountRow(name, accountRow)) {
            client_->Abort(timeout_);
            return false;
        }
        const uint32_t customerId = accountRow.customer_id();
        if (!ReadCheckingRow(customerId, checkingRow) || !
                ReadSavingRow(customerId, savingRow)) {
            client_->Abort(timeout_);
            return false;
        }
        const uint32_t sum = checkingRow.checking_balance() + savingRow.saving_balance();
        if (sum < value) {
            InsertCheckingRow(customerId, sum - value - 1);
        } else {
            InsertCheckingRow(customerId, sum - value);
        }
        client_->Commit(timeout_);
        return true;
    }

    void SmallBankTransaction::InsertAccountRow(const std::string &name, const uint32_t customer_id) {
        proto::AccountRow accountRow;
        accountRow.set_name(name);
        accountRow.set_customer_id(customer_id);
        std::string accountRowSerialized;
        accountRow.SerializeToString(&accountRowSerialized);
        std::string accountRowKey = AccountRowKey(name);
        client_->Put(accountRowKey, accountRowSerialized, timeout_);
    }

    void SmallBankTransaction::InsertSavingRow(const uint32_t customer_id, const uint32_t balance) {
        proto::SavingRow savingRow;
        savingRow.set_customer_id(customer_id);
        savingRow.set_saving_balance(balance);
        std::string savingRowSerialized;
        savingRow.SerializeToString(&savingRowSerialized);
        std::string savingRowKey = SavingRowKey(customer_id);
        client_->Put(savingRowKey, savingRowSerialized, timeout_);
    }

    void SmallBankTransaction::InsertCheckingRow(const uint32_t customer_id, const uint32_t balance) {
        proto::CheckingRow checkingRow;
        checkingRow.set_customer_id(customer_id);
        checkingRow.set_checking_balance(balance);
        std::string checkingRowSerialized;
        checkingRow.SerializeToString(&checkingRowSerialized);
        std::string checkingRowKey = CheckingRowKey(customer_id);
        client_->Put(checkingRowKey, checkingRowSerialized, timeout_);
    }

    bool SmallBankTransaction::ReadAccountRow(const std::string &name, proto::AccountRow &accountRow) {
        std::string accountRowKey = AccountRowKey(name);
        std::string accountRowSerialized;
        client_->Get(accountRowKey, accountRowSerialized, timeout_);
        return accountRow.ParseFromString(accountRowSerialized);
    }

    bool SmallBankTransaction::ReadSavingRow(const uint32_t customer_id, proto::SavingRow &savingRow) {
        std::string savingRowKey = SavingRowKey(customer_id);
        std::string savingRowSerialized;
        client_->Get(savingRowKey, savingRowSerialized, timeout_);
        return savingRow.ParseFromString(savingRowSerialized);
    }

    bool SmallBankTransaction::ReadCheckingRow(const uint32_t customer_id, proto::CheckingRow &checkingRow) {
        std::string checkingRowKey = CheckingRowKey(customer_id);
        std::string checkingRowSerialized;
        client_->Get(checkingRowKey, checkingRowSerialized, timeout_);
        return checkingRow.ParseFromString(checkingRowSerialized);
    }
}

