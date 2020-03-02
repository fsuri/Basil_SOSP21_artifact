#include "store/benchmark/async/smallbank/utils.h"
#include "store/benchmark/async/smallbank/smallbank-proto.pb.h"

namespace smallbank {

    std::string AccountRowKey(const std::string name) {
        char keyC[1];
        keyC[0] = static_cast<char>(proto::Tables::ACCOUNT);
        return std::string(keyC, sizeof(keyC)) + name;
    }

    std::string SavingRowKey(const uint32_t customer_id) {
        char keyC[5];
        keyC[0] = static_cast<char>(proto::Tables::SAVING);
        *reinterpret_cast<uint32_t *>(keyC + 1) = customer_id;
        return std::string(keyC, sizeof(keyC));
    }

    std::string CheckingRowKey(const uint32_t customer_id) {
        char keyC[5];
        keyC[0] = static_cast<char>(proto::Tables::CHECKING);
        *reinterpret_cast<uint32_t *>(keyC + 1) = customer_id;
        return std::string(keyC, sizeof(keyC));
    }

    void InsertAccountRow(SyncClient &client, const std::string &name, const uint32_t customer_id, const uint32_t timeout) {
        proto::AccountRow accountRow;
        accountRow.set_name(name);
        accountRow.set_customer_id(customer_id);
        std::string accountRowSerialized;
        accountRow.SerializeToString(&accountRowSerialized);
        std::string accountRowKey = AccountRowKey(name);
        client.Put(accountRowKey, accountRowSerialized, timeout);
    }

    void InsertSavingRow(SyncClient &client, const uint32_t customer_id, const uint32_t balance, const uint32_t timeout) {
        proto::SavingRow savingRow;
        savingRow.set_customer_id(customer_id);
        savingRow.set_saving_balance(balance);
        std::string savingRowSerialized;
        savingRow.SerializeToString(&savingRowSerialized);
        std::string savingRowKey = SavingRowKey(customer_id);
        client.Put(savingRowKey, savingRowSerialized, timeout);
    }

    void InsertCheckingRow(SyncClient &client, const uint32_t customer_id, const uint32_t balance, const uint32_t timeout) {
        proto::CheckingRow checkingRow;
        checkingRow.set_customer_id(customer_id);
        checkingRow.set_checking_balance(balance);
        std::string checkingRowSerialized;
        checkingRow.SerializeToString(&checkingRowSerialized);
        std::string checkingRowKey = CheckingRowKey(customer_id);
        client.Put(checkingRowKey, checkingRowSerialized, timeout);
    }

    bool ReadAccountRow(SyncClient &client, const std::string &name, proto::AccountRow &accountRow, const uint32_t timeout) {
        std::string accountRowKey = AccountRowKey(name);
        std::string accountRowSerialized;
        client.Get(accountRowKey, accountRowSerialized, timeout);
        return accountRow.ParseFromString(accountRowSerialized);
    }

    bool ReadSavingRow(SyncClient &client, const uint32_t customer_id, proto::SavingRow &savingRow, const uint32_t timeout) {
        std::string savingRowKey = SavingRowKey(customer_id);
        std::string savingRowSerialized;
        client.Get(savingRowKey, savingRowSerialized, timeout);
        return savingRow.ParseFromString(savingRowSerialized);
    }

    bool ReadCheckingRow(SyncClient &client, const uint32_t customer_id, proto::CheckingRow &checkingRow, const uint32_t timeout) {
        std::string checkingRowKey = CheckingRowKey(customer_id);
        std::string checkingRowSerialized;
        client.Get(checkingRowKey, checkingRowSerialized, timeout);
        return checkingRow.ParseFromString(checkingRowSerialized);
    }
}
