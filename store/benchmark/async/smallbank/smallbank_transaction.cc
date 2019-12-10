// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 *   Smallbank transactions.
 *
 **********************************************************************/

#include "store/benchmark/async/smallbank/smallbank_transaction.h"
#include "store/benchmark/async/smallbank/smallbank-proto.pb.h"
#include "store/benchmark/async/smallbank/utils.h"
#include "store/common/frontend/sync_client.h"

namespace smallbank {

SmallbankTransaction::SmallbankTransaction(SmallbankTransactionType type,
    const std::string &cust1, const std::string &cust2,
    const uint32_t timeout) : SyncTransaction(0UL), type(type),
      cust1(cust1), cust2(cust2), timeout_(timeout) {
}

int SmallbankTransaction::Execute(SyncClient &client) {
  switch (type) {
    case BALANCE:
      return Bal(client, cust1).second ? 0 : -1;
    case DEPOSIT:
      return DepositChecking(client, cust1, rand() % 50 + 1) ? 0 : -1;
    case TRANSACT:
      return TransactSaving(client, cust1, rand() % 101 - 50) ? 0 : -1; 
    case AMALGAMATE:
      return Amalgamate(client, cust1, cust2) ? 0 : -1;
    case WRITE_CHECK:
      return WriteCheck(client, cust1, rand() % 50) ? 0 : -1;
    default:
      NOT_REACHABLE();
  }
}

std::pair<uint32_t, bool> SmallbankTransaction::Bal(SyncClient &client, const std::string &name) {
    proto::AccountRow accountRow;
    proto::SavingRow savingRow;
    proto::CheckingRow checkingRow;
    client.Begin();
    Debug("Balance for name %s", name.c_str());
    if (!ReadAccountRow(client, name, accountRow) || !ReadSavingRow(client, accountRow.customer_id(), savingRow) ||
        !ReadCheckingRow(client, accountRow.customer_id(), checkingRow)) {

        client.Abort(timeout_);
        Debug("Aborted Balance");
        return std::make_pair(0, false);
    }
    client.Commit(timeout_);
    Debug("Committed Balance %d", savingRow.saving_balance()
        + checkingRow.checking_balance());
    return std::make_pair(savingRow.saving_balance() + checkingRow.checking_balance(), true);
}

bool SmallbankTransaction::DepositChecking(SyncClient &client, const std::string &name, const int32_t value) {
    
    Debug("DepositChecking for name %s with val %d", name.c_str(), value);
    if (value < 0) {
        client.Abort(timeout_);
        Debug("Aborted DepositChecking (- val)");
        return false;
    }
    proto::AccountRow accountRow;
    proto::CheckingRow checkingRow;

    client.Begin();
    if (!ReadAccountRow(client, name, accountRow)) {
        client.Abort(timeout_);
        Debug("Aborted DepositChecking (AccountRow)");
        return false;
    }
    const uint32_t customerId = accountRow.customer_id();
    if (!ReadCheckingRow(client, customerId, checkingRow)) {
        client.Abort(timeout_);
        Debug("Aborted DepositChecking (CheckingRow)");
        return false;
    }
    Debug("DepositChecking old value %d", checkingRow.checking_balance());
    InsertCheckingRow(client, customerId, checkingRow.checking_balance() + value);
    client.Commit(timeout_);
    Debug("Committed DepositChecking %d", checkingRow.checking_balance());
    return true;
}

bool SmallbankTransaction::TransactSaving(SyncClient &client, const std::string &name, const int32_t value) {
    proto::SavingRow savingRow;
    proto::AccountRow accountRow;

    client.Begin();
    Debug("TransactSaving for name %s with val %d", name.c_str(), value);
    if (!ReadAccountRow(client, name, accountRow)) {
        client.Abort(timeout_);
        Debug("Aborted TransactSaving (AccountRow)");
        return false;
    }
    const uint32_t customerId = accountRow.customer_id();
    if (!ReadSavingRow(client, customerId, savingRow)) {
        client.Abort(timeout_);
        Debug("Aborted TransactSaving (SavingRow)");
        return false;
    }
    const int32_t balance = savingRow.saving_balance();
    Debug("TransactSaving old value %d", balance);
    const int resultingBalance = balance + value;
    Debug("TransactSaving resulting %d", resultingBalance);
    if (resultingBalance < 0) {
        client.Abort(timeout_);
        Debug("Aborted TransactSaving (Negative Result)");
        return false;
    }
    InsertSavingRow(client, customerId, resultingBalance);
    client.Commit(timeout_);
    Debug("Committed TransactSaving");
    return true;
}

bool SmallbankTransaction::Amalgamate(SyncClient &client, const std::string &name1, const std::string &name2) {
    proto::AccountRow accountRow1;
    proto::AccountRow accountRow2;

    proto::CheckingRow checkingRow1;
    proto::SavingRow savingRow1;
    proto::CheckingRow checkingRow2;

    client.Begin();
    Debug("Amalgamate for names %s %s", name1.c_str(), name2.c_str());
    if (!ReadAccountRow(client, name1, accountRow1) || !ReadAccountRow(client, name2, accountRow2)) {
        client.Abort(timeout_);
        Debug( "Aborted Amalgamate (AccountRow)" );
        return false;
    }
    const uint32_t customerId1 = accountRow1.customer_id();
    const uint32_t customerId2 = accountRow2.customer_id();
    if (!ReadCheckingRow(client, customerId2, checkingRow2)) {
        client.Abort(timeout_);
        Debug( "Aborted Amalgamate (CheckingRow)" );
        return false;
    }
    const int32_t balance2 = checkingRow2.checking_balance();
    if (!ReadCheckingRow(client, customerId1, checkingRow1) || !
            ReadSavingRow(client, customerId1, savingRow1)) {
        client.Abort(timeout_);
        Debug( "Aborted Amalgamate (2nd Balance)" );
        return false;
    }
    InsertCheckingRow(client, customerId2, balance2 + checkingRow1.checking_balance() + savingRow1.saving_balance());
    InsertSavingRow(client, customerId1, 0);
    InsertCheckingRow(client, customerId1, 0);
    client.Commit(timeout_);
    Debug( "Committed Amalgamate" );
    return true;
}

bool SmallbankTransaction::WriteCheck(SyncClient &client, const std::string &name, const int32_t value) {
    proto::AccountRow accountRow;
    proto::CheckingRow checkingRow;
    proto::SavingRow savingRow;

    client.Begin();
    Debug("WriteCheck for name %s with value %d", name.c_str(), value);
    if (!ReadAccountRow(client, name, accountRow)) {
        client.Abort(timeout_);
        Debug("Aborted WriteCheck (AccountRow)");
        return false;
    }
    const uint32_t customerId = accountRow.customer_id();
    if (!ReadCheckingRow(client, customerId, checkingRow) || !
            ReadSavingRow(client, customerId, savingRow)) {
        client.Abort(timeout_);
        Debug("Aborted WriteCheck (Balance)");
        return false;
    }
    const int32_t sum = checkingRow.checking_balance() + savingRow.saving_balance();
    Debug("Sum for WriteCheck %d", sum);
    if (sum < value) {
        InsertCheckingRow(client, customerId, checkingRow.checking_balance() - value - 1);
    } else {
        InsertCheckingRow(client, customerId, checkingRow.checking_balance() - value);
    }
    client.Commit(timeout_);
    Debug("Committed WriteCheck (AccountRow)");
    return true;
}

void SmallbankTransaction::InsertAccountRow(SyncClient &client, const std::string &name, const uint32_t customer_id) {
    proto::AccountRow accountRow;
    accountRow.set_name(name);
    accountRow.set_customer_id(customer_id);
    std::string accountRowSerialized;
    accountRow.SerializeToString(&accountRowSerialized);
    std::string accountRowKey = AccountRowKey(name);
    client.Put(accountRowKey, accountRowSerialized, timeout_);
}

void SmallbankTransaction::InsertSavingRow(SyncClient &client, const uint32_t customer_id, const uint32_t balance) {
    proto::SavingRow savingRow;
    savingRow.set_customer_id(customer_id);
    savingRow.set_saving_balance(balance);
    std::string savingRowSerialized;
    savingRow.SerializeToString(&savingRowSerialized);
    std::string savingRowKey = SavingRowKey(customer_id);
    client.Put(savingRowKey, savingRowSerialized, timeout_);
}

void SmallbankTransaction::InsertCheckingRow(SyncClient &client, const uint32_t customer_id, const uint32_t balance) {
    proto::CheckingRow checkingRow;
    checkingRow.set_customer_id(customer_id);
    checkingRow.set_checking_balance(balance);
    std::string checkingRowSerialized;
    checkingRow.SerializeToString(&checkingRowSerialized);
    std::string checkingRowKey = CheckingRowKey(customer_id);
    client.Put(checkingRowKey, checkingRowSerialized, timeout_);
}

bool SmallbankTransaction::ReadAccountRow(SyncClient &client, const std::string &name, proto::AccountRow &accountRow) {
    std::string accountRowKey = AccountRowKey(name);
    std::string accountRowSerialized;
    client.Get(accountRowKey, accountRowSerialized, timeout_);
    return accountRow.ParseFromString(accountRowSerialized);
}

bool SmallbankTransaction::ReadSavingRow(SyncClient &client, const uint32_t customer_id, proto::SavingRow &savingRow) {
    std::string savingRowKey = SavingRowKey(customer_id);
    std::string savingRowSerialized;
    client.Get(savingRowKey, savingRowSerialized, timeout_);
    return savingRow.ParseFromString(savingRowSerialized);
}

bool SmallbankTransaction::ReadCheckingRow(SyncClient &client, const uint32_t customer_id, proto::CheckingRow &checkingRow) {
    std::string checkingRowKey = CheckingRowKey(customer_id);
    std::string checkingRowSerialized;
    client.Get(checkingRowKey, checkingRowSerialized, timeout_);
    return checkingRow.ParseFromString(checkingRowSerialized);
}

SmallbankTransactionType SmallbankTransaction::GetTransactionType() {
    return type;
}
}

