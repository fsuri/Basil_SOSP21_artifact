//
// Created by Janice Chan on 10/12/19.
//
#include "store/benchmark/async/smallbank/smallbank_transaction.h"
#include "store/benchmark/async/smallbank/tests/smallbank_test_utils.h"
#include "store/benchmark/async/smallbank/amalgamate.h"
#include "store/benchmark/async/smallbank/bal.h"
#include "store/benchmark/async/smallbank/deposit.h"
#include "store/benchmark/async/smallbank/smallbank_transaction.h"
#include "store/benchmark/async/smallbank/transact.h"
#include "store/benchmark/async/smallbank/utils.h"
#include "store/benchmark/async/smallbank/write_check.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace smallbank {
TEST(Bal, ReadAccountFailure) {
  std::string cust = "cust1";
  int timeout = 0;

  MockSyncClient mockSyncClient;
  Bal smallbankTransaction(cust, timeout);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(cust), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(timeout)).Times(1);
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), -1);
}

TEST(Bal, ReadSavingFailure) {
  std::string cust = "cust1";
  uint32_t customerId = 10;
  int timeout = 0;

  MockSyncClient mockSyncClient;
  Bal smallbankTransaction(cust, timeout);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  proto::AccountRow accountRow;
  accountRow.set_name(cust);
  accountRow.set_customer_id(customerId);
  std::string accountRowSerialized;
  accountRow.SerializeToString(&accountRowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(cust), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(accountRowSerialized));
  EXPECT_CALL(mockSyncClient,
              Get(SavingRowKey(customerId), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(timeout)).Times(1);
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), -1);
}

TEST(Bal, ReadCheckingFailure) {
  std::string cust = "cust1";
  uint32_t customerId = 10;
  int timeout = 0;
  int savingBalance = 200;

  MockSyncClient mockSyncClient;
  Bal smallbankTransaction(cust, timeout);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  proto::AccountRow accountRow;
  accountRow.set_name(cust);
  accountRow.set_customer_id(customerId);
  std::string accountRowSerialized;
  accountRow.SerializeToString(&accountRowSerialized);
  proto::SavingRow savingRow;
  savingRow.set_customer_id(customerId);
  savingRow.set_saving_balance(savingBalance);
  std::string savingRowSerialized;
  savingRow.SerializeToString(&savingRowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(cust), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(accountRowSerialized));
  EXPECT_CALL(mockSyncClient,
              Get(SavingRowKey(customerId), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(savingRowSerialized));
  EXPECT_CALL(mockSyncClient,
              Get(CheckingRowKey(customerId), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(timeout)).Times(1);
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), -1);
}
TEST(Bal, Success) {
  std::string cust = "cust1";
  uint32_t customerId = 10;
  int timeout = 0;
  int savingBalance = 200;
  int checkingBalance = 1000;

  MockSyncClient mockSyncClient;
  Bal smallbankTransaction(cust, timeout);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  proto::AccountRow accountRow;
  accountRow.set_name(cust);
  accountRow.set_customer_id(customerId);
  std::string accountRowSerialized;
  accountRow.SerializeToString(&accountRowSerialized);
  proto::SavingRow savingRow;
  savingRow.set_customer_id(customerId);
  savingRow.set_saving_balance(savingBalance);
  std::string savingRowSerialized;
  savingRow.SerializeToString(&savingRowSerialized);
  proto::CheckingRow checkingRow;
  checkingRow.set_customer_id(customerId);
  checkingRow.set_checking_balance(checkingBalance);
  std::string checkingRowSerialized;
  checkingRow.SerializeToString(&checkingRowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(cust), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(accountRowSerialized));
  EXPECT_CALL(mockSyncClient,
              Get(SavingRowKey(customerId), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(savingRowSerialized));
  EXPECT_CALL(mockSyncClient,
              Get(CheckingRowKey(customerId), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(checkingRowSerialized));
  EXPECT_CALL(mockSyncClient, Commit(timeout)).Times(1);
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), 0);
}

TEST(DepositChecking, NegativeDeposit) {
  MockSyncClient mockSyncClient;
  DepositChecking smallbankTransaction("cust1", -18000, 0);
  EXPECT_CALL(mockSyncClient, Abort(0)).Times(1);
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), -1);
}

TEST(DepositChecking, ReadAccountFailure) {
  MockSyncClient mockSyncClient;
  DepositChecking smallbankTransaction("cust1", 18000, 0);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey("cust1"), testing::_, 0))
      .WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(0)).Times(1);
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), -1);
}

TEST(DepositChecking, ReadCheckingFailure) {
  std::string cust = "cust1";
  uint32_t customerId = 10;
  int timeout = 0;

  MockSyncClient mockSyncClient;
  DepositChecking smallbankTransaction("cust1", 18000, timeout);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  proto::AccountRow accountRow;
  accountRow.set_name(cust);
  accountRow.set_customer_id(customerId);
  std::string accountRowSerialized;
  accountRow.SerializeToString(&accountRowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(cust), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(accountRowSerialized));
  EXPECT_CALL(mockSyncClient,
              Get(CheckingRowKey(customerId), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(timeout)).Times(1);
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), -1);
}

TEST(DepositChecking, Success) {
  std::string cust = "cust1";
  uint32_t customerId = 10;
  int timeout = 0;
  int deposit = 180000;
  int checkingBalance = 120;
  MockSyncClient mockSyncClient;
  DepositChecking smallbankTransaction(cust, deposit, timeout);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  proto::AccountRow accountRow;
  accountRow.set_name(cust);
  accountRow.set_customer_id(customerId);
  std::string accountRowSerialized;
  accountRow.SerializeToString(&accountRowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(cust), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(accountRowSerialized));
  proto::CheckingRow checkingRow;
  checkingRow.set_checking_balance(checkingBalance);
  checkingRow.set_customer_id(customerId);
  std::string checkingRowSerialized;
  checkingRow.SerializeToString(&checkingRowSerialized);
  EXPECT_CALL(mockSyncClient,
              Get(CheckingRowKey(customerId), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(checkingRowSerialized));
  proto::CheckingRow newCheckingRow;
  newCheckingRow.set_checking_balance(checkingBalance + deposit);
  newCheckingRow.set_customer_id(customerId);
  std::string newCheckingRowSerialized;
  newCheckingRow.SerializeToString(&newCheckingRowSerialized);
  EXPECT_CALL(mockSyncClient, Put(CheckingRowKey(customerId),
                                  newCheckingRowSerialized, timeout))
      .Times(1);
  EXPECT_CALL(mockSyncClient, Commit(timeout)).Times(1);
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), 0);
}

TEST(TransactSaving, ReadAccountFailure) {
  MockSyncClient mockSyncClient;
  TransactSaving smallbankTransaction("cust1", 18000, 0);

  EXPECT_CALL(mockSyncClient, Begin()).Times(1);

  EXPECT_CALL(mockSyncClient, Get(AccountRowKey("cust1"), testing::_, 0))
      .WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(0)).Times(1);
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), -1);
}

TEST(TransactSaving, ReadSavingFailure) {
  MockSyncClient mockSyncClient;
  uint32_t customerId = 10;
  std::string cust = "cust1";
  uint32_t timeout = 0;

  TransactSaving smallbankTransaction(cust, 18000, timeout);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  proto::AccountRow accountRow;
  accountRow.set_name(cust);
  accountRow.set_customer_id(customerId);
  std::string accountRowSerialized;
  accountRow.SerializeToString(&accountRowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(cust), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(accountRowSerialized));
  EXPECT_CALL(mockSyncClient,
              Get(SavingRowKey(customerId), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(timeout)).Times(1);
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), -1);
}

TEST(TransactSaving, ResultingBalanceNegative) {
  MockSyncClient mockSyncClient;
  uint32_t customerId = 10;
  std::string cust = "cust1";
  uint32_t timeout = 0;
  uint32_t savingBalance = 150;

  TransactSaving smallbankTransaction(cust, -151, 0);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  proto::AccountRow accountRow;
  accountRow.set_name(cust);
  accountRow.set_customer_id(customerId);
  std::string accountRowSerialized;
  accountRow.SerializeToString(&accountRowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(cust), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(accountRowSerialized));
  proto::SavingRow savingRow;
  savingRow.set_saving_balance(savingBalance);
  savingRow.set_customer_id(customerId);
  std::string savingRowSerialized;
  savingRow.SerializeToString(&savingRowSerialized);
  EXPECT_CALL(mockSyncClient,
              Get(SavingRowKey(customerId), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(savingRowSerialized));
  EXPECT_CALL(mockSyncClient, Abort(timeout)).Times(1);
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), -1);
}

TEST(TransactSaving, Success) {
  MockSyncClient mockSyncClient;
  uint32_t customerId = 10;
  std::string cust = "cust1";
  uint32_t timeout = 0;
  uint32_t savingBalance = 150;
  int transact = -150;

  TransactSaving smallbankTransaction(cust, transact, 0);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  proto::AccountRow accountRow;
  accountRow.set_name(cust);
  accountRow.set_customer_id(customerId);
  std::string accountRowSerialized;
  accountRow.SerializeToString(&accountRowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(cust), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(accountRowSerialized));
  proto::SavingRow savingRow;
  savingRow.set_saving_balance(savingBalance);
  savingRow.set_customer_id(customerId);
  std::string savingRowSerialized;
  savingRow.SerializeToString(&savingRowSerialized);
  EXPECT_CALL(mockSyncClient,
              Get(SavingRowKey(customerId), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(savingRowSerialized));
  EXPECT_CALL(mockSyncClient, Commit(timeout)).Times(1);
  proto::SavingRow newSavingRow;
  newSavingRow.set_saving_balance(savingBalance + transact);
  newSavingRow.set_customer_id(customerId);
  std::string newSavingRowSerialized;
  newSavingRow.SerializeToString(&newSavingRowSerialized);
  EXPECT_CALL(mockSyncClient,
              Put(SavingRowKey(customerId), newSavingRowSerialized, timeout))
      .Times(1);
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), 0);
}

TEST(Amalgamate, ReadAccount1Failure) {
  MockSyncClient mockSyncClient;
  std::string cust1 = "cust1";
  std::string cust2 = "cust2";
  uint32_t timeout = 0;
  Amalgamate smallbankTransaction(cust1, cust2, timeout);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);

  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(cust1), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(0)).Times(1);
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), -1);
}

TEST(Amalgamate, ReadAccount2Failure) {
  MockSyncClient mockSyncClient;
  std::string cust1 = "cust1";
  std::string cust2 = "cust2";
  uint32_t customerId1 = 100;
  uint32_t timeout = 0;
  Amalgamate smallbankTransaction(cust1, cust2, timeout);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);

  proto::AccountRow account1Row;
  account1Row.set_name(cust1);
  account1Row.set_customer_id(customerId1);
  std::string account1RowSerialized;
  account1Row.SerializeToString(&account1RowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(cust1), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(account1RowSerialized));
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(cust2), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(0)).Times(1);
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), -1);
}
TEST(Amalgamate, ReadChecking2Failure) {
  MockSyncClient mockSyncClient;
  std::string cust1 = "cust1";
  std::string cust2 = "cust2";
  uint32_t customerId1 = 100;
  uint32_t customerId2 = 200;
  uint32_t timeout = 0;
  Amalgamate smallbankTransaction(cust1, cust2, timeout);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);

  proto::AccountRow account1Row;
  account1Row.set_name(cust1);
  account1Row.set_customer_id(customerId1);
  std::string account1RowSerialized;
  account1Row.SerializeToString(&account1RowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(cust1), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(account1RowSerialized));
  proto::AccountRow account2Row;
  account2Row.set_name(cust2);
  account2Row.set_customer_id(customerId2);
  std::string account2RowSerialized;
  account2Row.SerializeToString(&account2RowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(cust2), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(account2RowSerialized));
  EXPECT_CALL(mockSyncClient,
              Get(CheckingRowKey(customerId2), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(timeout)).Times(1);
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), -1);
}

TEST(Amalgamate, ReadChecking1Failure) {
  MockSyncClient mockSyncClient;
  std::string cust1 = "cust1";
  std::string cust2 = "cust2";
  uint32_t checking2Balance = 50;
  uint32_t customerId1 = 100;
  uint32_t customerId2 = 200;
  uint32_t timeout = 0;
  Amalgamate smallbankTransaction(cust1, cust2, timeout);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);

  proto::AccountRow account1Row;
  account1Row.set_name(cust1);
  account1Row.set_customer_id(customerId1);
  std::string account1RowSerialized;
  account1Row.SerializeToString(&account1RowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(cust1), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(account1RowSerialized));
  proto::AccountRow account2Row;
  account2Row.set_name(cust2);
  account2Row.set_customer_id(customerId2);
  std::string account2RowSerialized;
  account2Row.SerializeToString(&account2RowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(cust2), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(account2RowSerialized));
  proto::CheckingRow checking2Row;
  checking2Row.set_checking_balance(checking2Balance);
  checking2Row.set_customer_id(customerId2);
  std::string checking2RowSerialized;
  checking2Row.SerializeToString(&checking2RowSerialized);
  EXPECT_CALL(mockSyncClient,
              Get(CheckingRowKey(customerId2), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(checking2RowSerialized));
  EXPECT_CALL(mockSyncClient,
              Get(CheckingRowKey(customerId1), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(timeout)).Times(1);
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), -1);
}

TEST(Amalgamate, ReadSaving1Failure) {
  MockSyncClient mockSyncClient;
  std::string cust1 = "cust1";
  std::string cust2 = "cust2";
  uint32_t checking1Balance = 15;
  uint32_t checking2Balance = 50;
  uint32_t customerId1 = 100;
  uint32_t customerId2 = 200;
  uint32_t timeout = 0;
  Amalgamate smallbankTransaction(cust1, cust2, timeout);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);

  proto::AccountRow account1Row;
  account1Row.set_name(cust1);
  account1Row.set_customer_id(customerId1);
  std::string account1RowSerialized;
  account1Row.SerializeToString(&account1RowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(cust1), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(account1RowSerialized));
  proto::AccountRow account2Row;
  account2Row.set_name(cust2);
  account2Row.set_customer_id(customerId2);
  std::string account2RowSerialized;
  account2Row.SerializeToString(&account2RowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(cust2), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(account2RowSerialized));
  proto::CheckingRow checking2Row;
  checking2Row.set_checking_balance(checking2Balance);
  checking2Row.set_customer_id(customerId2);
  std::string checking2RowSerialized;
  checking2Row.SerializeToString(&checking2RowSerialized);
  proto::CheckingRow checking1Row;
  checking1Row.set_checking_balance(checking1Balance);
  checking1Row.set_customer_id(customerId1);
  std::string checking1RowSerialized;
  checking1Row.SerializeToString(&checking1RowSerialized);
  EXPECT_CALL(mockSyncClient,
              Get(CheckingRowKey(customerId2), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(checking2RowSerialized));
  EXPECT_CALL(mockSyncClient,
              Get(CheckingRowKey(customerId1), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(checking1RowSerialized));
  EXPECT_CALL(mockSyncClient,
              Get(SavingRowKey(customerId1), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(timeout)).Times(1);
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), -1);
}

TEST(Amalgamate, Success) {
  MockSyncClient mockSyncClient;
  std::string cust1 = "cust1";
  std::string cust2 = "cust2";
  uint32_t checking1Balance = 15;
  uint32_t checking2Balance = 50;
  uint32_t saving1Balance = 2002;
  uint32_t customerId1 = 100;
  uint32_t customerId2 = 200;
  uint32_t timeout = 0;
  Amalgamate smallbankTransaction(cust1, cust2, timeout);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);

  proto::AccountRow account1Row;
  account1Row.set_name(cust1);
  account1Row.set_customer_id(customerId1);
  std::string account1RowSerialized;
  account1Row.SerializeToString(&account1RowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(cust1), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(account1RowSerialized));
  proto::AccountRow account2Row;
  account2Row.set_name(cust2);
  account2Row.set_customer_id(customerId2);
  std::string account2RowSerialized;
  account2Row.SerializeToString(&account2RowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(cust2), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(account2RowSerialized));
  proto::CheckingRow checking2Row;
  checking2Row.set_checking_balance(checking2Balance);
  checking2Row.set_customer_id(customerId2);
  std::string checking2RowSerialized;
  checking2Row.SerializeToString(&checking2RowSerialized);
  proto::CheckingRow checking1Row;
  checking1Row.set_checking_balance(checking1Balance);
  checking1Row.set_customer_id(customerId1);
  std::string checking1RowSerialized;
  checking1Row.SerializeToString(&checking1RowSerialized);
  proto::SavingRow saving1Row;
  saving1Row.set_saving_balance(saving1Balance);
  saving1Row.set_customer_id(customerId1);
  std::string saving1RowSerialized;
  saving1Row.SerializeToString(&saving1RowSerialized);
  EXPECT_CALL(mockSyncClient,
              Get(CheckingRowKey(customerId2), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(checking2RowSerialized));
  EXPECT_CALL(mockSyncClient,
              Get(CheckingRowKey(customerId1), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(checking1RowSerialized));
  EXPECT_CALL(mockSyncClient,
              Get(SavingRowKey(customerId1), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(saving1RowSerialized));
  proto::CheckingRow newChecking1Row;
  newChecking1Row.set_checking_balance(0);
  newChecking1Row.set_customer_id(customerId1);
  std::string newChecking1RowSerialized;
  newChecking1Row.SerializeToString(&newChecking1RowSerialized);
  proto::SavingRow newSaving1Row;
  newSaving1Row.set_saving_balance(0);
  newSaving1Row.set_customer_id(customerId1);
  std::string newSaving1RowSerialized;
  newSaving1Row.SerializeToString(&newSaving1RowSerialized);
  proto::CheckingRow newChecking2Row;
  newChecking2Row.set_checking_balance(checking2Balance + checking1Balance +
                                       saving1Balance);
  newChecking2Row.set_customer_id(customerId2);
  std::string newChecking2RowSerialized;
  newChecking2Row.SerializeToString(&newChecking2RowSerialized);
  EXPECT_CALL(mockSyncClient, Put(CheckingRowKey(customerId1),
                                  newChecking1RowSerialized, timeout))
      .Times(1);
  EXPECT_CALL(mockSyncClient,
              Put(SavingRowKey(customerId1), newSaving1RowSerialized, timeout))
      .Times(1);
  EXPECT_CALL(mockSyncClient, Put(CheckingRowKey(customerId2),
                                  newChecking2RowSerialized, timeout))
      .Times(1);
  EXPECT_CALL(mockSyncClient, Commit(timeout)).Times(1);
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), 0);
}

TEST(WriteCheck, ReadAccountFailure) {
  MockSyncClient mockSyncClient;
  WriteCheck smallbankTransaction("cust1", 18000, 0);

  EXPECT_CALL(mockSyncClient, Begin()).Times(1);

  EXPECT_CALL(mockSyncClient, Get(AccountRowKey("cust1"), testing::_, 0))
      .WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(0)).Times(1);
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), -1);
}

TEST(WriteCheck, ReadCheckingFailure) {
  MockSyncClient mockSyncClient;
  uint32_t customerId = 10;
  std::string cust = "cust1";
  uint32_t timeout = 0;

  WriteCheck smallbankTransaction(cust, 18000, timeout);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  proto::AccountRow accountRow;
  accountRow.set_name(cust);
  accountRow.set_customer_id(customerId);
  std::string accountRowSerialized;
  accountRow.SerializeToString(&accountRowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(cust), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(accountRowSerialized));
  EXPECT_CALL(mockSyncClient,
              Get(CheckingRowKey(customerId), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(timeout)).Times(1);
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), -1);
}
TEST(WriteCheck, ReadSavingFailure) {
  MockSyncClient mockSyncClient;
  uint32_t customerId = 10;
  std::string cust = "cust1";
  uint32_t timeout = 0;
  uint32_t checkingBalance = 150;

  WriteCheck smallbankTransaction(cust, 18000, timeout);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  proto::AccountRow accountRow;
  accountRow.set_name(cust);
  accountRow.set_customer_id(customerId);
  std::string accountRowSerialized;
  accountRow.SerializeToString(&accountRowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(cust), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(accountRowSerialized));
  proto::CheckingRow checkingRow;
  checkingRow.set_checking_balance(checkingBalance);
  checkingRow.set_customer_id(customerId);
  std::string checkingRowSerialized;
  checkingRow.SerializeToString(&checkingRowSerialized);
  EXPECT_CALL(mockSyncClient,
              Get(CheckingRowKey(customerId), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(checkingRowSerialized));
  EXPECT_CALL(mockSyncClient,
              Get(SavingRowKey(customerId), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(timeout)).Times(1);
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), -1);
}

TEST(WriteCheck, ExactlySufficientFunds) {
  MockSyncClient mockSyncClient;
  uint32_t customerId = 10;
  std::string cust = "cust1";
  uint32_t timeout = 0;
  uint32_t checkingBalance = 150;
  uint32_t savingBalance = 200;
  uint32_t withdrawal = checkingBalance + savingBalance;

  WriteCheck smallbankTransaction(cust, withdrawal, timeout);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  proto::AccountRow accountRow;
  accountRow.set_name(cust);
  accountRow.set_customer_id(customerId);
  std::string accountRowSerialized;
  accountRow.SerializeToString(&accountRowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(cust), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(accountRowSerialized));
  proto::CheckingRow checkingRow;
  checkingRow.set_checking_balance(checkingBalance);
  checkingRow.set_customer_id(customerId);
  std::string checkingRowSerialized;
  checkingRow.SerializeToString(&checkingRowSerialized);
  proto::SavingRow savingRow;
  savingRow.set_saving_balance(savingBalance);
  savingRow.set_customer_id(customerId);
  std::string savingRowSerialized;
  savingRow.SerializeToString(&savingRowSerialized);
  EXPECT_CALL(mockSyncClient,
              Get(CheckingRowKey(customerId), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(checkingRowSerialized));
  EXPECT_CALL(mockSyncClient,
              Get(SavingRowKey(customerId), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(savingRowSerialized));
  proto::CheckingRow newCheckingRow;
  newCheckingRow.set_checking_balance(checkingBalance - withdrawal);
  newCheckingRow.set_customer_id(customerId);
  std::string newCheckingRowSerialized;
  newCheckingRow.SerializeToString(&newCheckingRowSerialized);
  EXPECT_CALL(mockSyncClient, Put(CheckingRowKey(customerId),
                                  newCheckingRowSerialized, timeout))
      .Times(1);
  EXPECT_CALL(mockSyncClient, Commit(timeout)).Times(1);
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), 0);
}

TEST(WriteCheck, InsufficientFunds) {
  MockSyncClient mockSyncClient;
  uint32_t customerId = 10;
  std::string cust = "cust1";
  uint32_t timeout = 0;
  uint32_t checkingBalance = 150;
  uint32_t savingBalance = 200;
  uint32_t withdrawal = checkingBalance + savingBalance + 1000;

  WriteCheck smallbankTransaction(cust, withdrawal, timeout);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  proto::AccountRow accountRow;
  accountRow.set_name(cust);
  accountRow.set_customer_id(customerId);
  std::string accountRowSerialized;
  accountRow.SerializeToString(&accountRowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(cust), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(accountRowSerialized));
  proto::CheckingRow checkingRow;
  checkingRow.set_checking_balance(checkingBalance);
  checkingRow.set_customer_id(customerId);
  std::string checkingRowSerialized;
  checkingRow.SerializeToString(&checkingRowSerialized);
  proto::SavingRow savingRow;
  savingRow.set_saving_balance(savingBalance);
  savingRow.set_customer_id(customerId);
  std::string savingRowSerialized;
  savingRow.SerializeToString(&savingRowSerialized);
  EXPECT_CALL(mockSyncClient,
              Get(CheckingRowKey(customerId), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(checkingRowSerialized));
  EXPECT_CALL(mockSyncClient,
              Get(SavingRowKey(customerId), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(savingRowSerialized));
  proto::CheckingRow newCheckingRow;
  // contains penalty for overdrawing
  newCheckingRow.set_checking_balance(checkingBalance - (withdrawal + 1));
  newCheckingRow.set_customer_id(customerId);
  std::string newCheckingRowSerialized;
  newCheckingRow.SerializeToString(&newCheckingRowSerialized);
  EXPECT_CALL(mockSyncClient, Put(CheckingRowKey(customerId),
                                  newCheckingRowSerialized, timeout))
      .Times(1);
  EXPECT_CALL(mockSyncClient, Commit(timeout)).Times(1);
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), 0);
}

}  // namespace smallbank
