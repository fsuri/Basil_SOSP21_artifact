// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 *   SmallBank benchmarking client for a distributed transactional store.
 *
 **********************************************************************/

#include "store/common/frontend/client.h"
#include "store/benchmark/async/smallbank/client.h"

namespace smallbank {
    Benchmark::Benchmark(Client &client) : client_(client) {}
}

//    AccountRow Benchmark::ReadAccountRow(const& std::string key) {
//        int ret;
//        // statusor<
//
//    return(client_.Get(AccountRowKey(key)));
//    }
//    SavingRow Benchmark::ReadSavingRow(const& std::string key) {
//    return(client_.Get(SavingRowKey(key)));
//    }
//    CheckingRow Benchmark::ReadCheckingRow(const& std::string key) {
//    return(client_.Get(CheckingRowKey(key)));
//    }
//
//    void Benchmark::InsertAccountRow(const& std::string key, const& std::string name) {
//
//    }
//    void Benchmark::InsertSavingRow(const& std::string key, const& uint32_t balance);
//    void Benchmark::InsertCheckingRow(const& std::string key,  const& uint32_t balance);
//
//    void Benchmark::UpdateSavingRow(const& std::string key, const& uint32_t balance);
//    void Benchmark::UpdateCheckingRow(const& std::string key, const& uint32_t balance);




//CreateAccount(Name, customerId  )
//    insert row in all tables
//
//Bal(Name)
//    ReadAccountRow(name)
//        account.customerId
//        ReadSavingRow(customerId)
//        ReadCheckingRow(customerId)
//        add saving.balance+checking.balance
//
//DepositChecking(Name, int32 Value)
//    if value < 0 return error
//    ReadAccountRow(Name)
//    if name not in table, return error
//    account.customerId
//    UpdateCheckingRow(customerId, ReadCheckingRow(customerId).balance + Value)
//
//TransactSaving(Name, int32 Value)
//    if name not in table, return error
//    if value < 0
//    account.customerId
//    ReadSavingRow(customerId).balance
//    if balance + value < 0
//        return error
//    UpdateSavingRow(customerId, value + balance)
//
//Amalgamate(name1, name2)
//    ReadAccountRow(name1).customerId
//    ReadAccountRow(name2).customerId
//    value = ReadCheckingRow(CustomerId2).balance
//    UpdateCheckingRow(CustomerId2, value + ReadCheckingRow(customerId).balance + ReadSavingRow(customerId).balance)
//    UpdateSavingRow(customerId, 0)
//    UpdateCheckingRow(customerId, 0)
//
//WriteCheck(name, value)
//    ReadAccountRow(name).customerId
//    ReadCheckingRow(customerId).balance + ReadSavingRow(customerId).balance
//    if sum < value then
//        UpdateCheckingRow(customerId, checking - value - 1)
//    else
//        UpdateCheckingRow(customerId, checking - value)



