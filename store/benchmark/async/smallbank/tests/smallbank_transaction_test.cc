//
// Created by Janice Chan on 10/12/19.
//
#include "store/benchmark/async/smallbank/smallbank_transaction.h"
#include "store/benchmark/async/smallbank/utils.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace smallbank {

class MockSyncClient : public SyncClient {
 public:
  MockSyncClient() : SyncClient(nullptr) { }
  MOCK_METHOD(void, Begin, (), (override));
  MOCK_METHOD(void, Get, (const std::string &key, std::string &val,
      uint32_t timeout), (override));
  MOCK_METHOD(void, Put, (const std::string &key, const std::string &val,
      uint32_t timeout), (override));
  MOCK_METHOD(int, Commit, (uint32_t timeout), (override));
  MOCK_METHOD(void, Abort, (uint32_t timeout), (override));
};


TEST(Bal, ReadAccountFailure) {
  std::string name = "cust1";
  int timeout = 0;

  MockSyncClient mockSyncClient;
  SmallbankTransaction smallbankTransaction(BALANCE, name, "", timeout);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(name), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(timeout)).Times(1);
  auto bal = smallbankTransaction.Bal(mockSyncClient, name);
  EXPECT_FALSE(bal.second);
}

TEST(Bal, ReadSavingFailure) {
  std::string name = "cust1";
  uint32_t customerId = 10;
  int timeout = 0;

  MockSyncClient mockSyncClient;
  SmallbankTransaction smallbankTransaction(BALANCE, name, "", timeout);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  proto::AccountRow accountRow;
  accountRow.set_name(name);
  accountRow.set_customer_id(customerId);
  std::string accountRowSerialized;
  accountRow.SerializeToString(&accountRowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(name), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(accountRowSerialized));
  EXPECT_CALL(mockSyncClient, Get(SavingRowKey(customerId), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(timeout)).Times(1);
  auto bal = smallbankTransaction.Bal(mockSyncClient, name);
  EXPECT_FALSE(bal.second);
}

TEST(Bal, ReadCheckingFailure) {
  std::string name = "cust1";
  uint32_t customerId = 10;
  int timeout = 0;
  int savingBalance = 200;

  MockSyncClient mockSyncClient;
  SmallbankTransaction smallbankTransaction(BALANCE, name, "", timeout);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  proto::AccountRow accountRow;
  accountRow.set_name(name);
  accountRow.set_customer_id(customerId);
  std::string accountRowSerialized;
  accountRow.SerializeToString(&accountRowSerialized);
  proto::SavingRow savingRow;
  savingRow.set_customer_id(customerId);
  savingRow.set_saving_balance(savingBalance);
  std::string savingRowSerialized;
  savingRow.SerializeToString(&savingRowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(name), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(accountRowSerialized));
  EXPECT_CALL(mockSyncClient, Get(SavingRowKey(customerId), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(savingRowSerialized));
  EXPECT_CALL(mockSyncClient, Get(CheckingRowKey(customerId), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(timeout)).Times(1);
  auto bal = smallbankTransaction.Bal(mockSyncClient, name);
  EXPECT_FALSE(bal.second);

}
TEST(Bal, Success) {
  std::string name = "cust1";
  uint32_t customerId = 10;
  int timeout = 0;
  int savingBalance = 200;
  int checkingBalance = 1000;

  MockSyncClient mockSyncClient;
  SmallbankTransaction smallbankTransaction(BALANCE, name, "", timeout);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  proto::AccountRow accountRow;
  accountRow.set_name(name);
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
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(name), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(accountRowSerialized));
  EXPECT_CALL(mockSyncClient, Get(SavingRowKey(customerId), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(savingRowSerialized));
  EXPECT_CALL(mockSyncClient, Get(CheckingRowKey(customerId), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(checkingRowSerialized));
  EXPECT_CALL(mockSyncClient, Commit(timeout)).Times(1);
  auto bal = smallbankTransaction.Bal(mockSyncClient, name);
  EXPECT_TRUE(bal.second);
  EXPECT_EQ(bal.first, savingBalance + checkingBalance);
}

TEST(DepositChecking, NegativeDeposit) {
  MockSyncClient mockSyncClient;
  SmallbankTransaction smallbankTransaction(DEPOSIT, "cust1", "", 0);
  EXPECT_CALL(mockSyncClient, Abort(0)).Times(1);
  EXPECT_FALSE(smallbankTransaction.DepositChecking(mockSyncClient, "cust1", -18000));
}

TEST(DepositChecking, ReadAccountFailure) {
  MockSyncClient mockSyncClient;
  SmallbankTransaction smallbankTransaction(DEPOSIT, "cust1", "", 0);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey("cust1"), testing::_, 0)).WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(0)).Times(1);
  EXPECT_FALSE(smallbankTransaction.DepositChecking(mockSyncClient, "cust1", 18000));
}

TEST(DepositChecking, ReadCheckingFailure) {
  std::string name = "cust1";
  uint32_t customerId = 10;
  int timeout = 0;

  MockSyncClient mockSyncClient;
  SmallbankTransaction smallbankTransaction(DEPOSIT, name, "", timeout);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  proto::AccountRow accountRow;
  accountRow.set_name(name);
  accountRow.set_customer_id(customerId);
  std::string accountRowSerialized;
  accountRow.SerializeToString(&accountRowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(name), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(accountRowSerialized));
  EXPECT_CALL(mockSyncClient, Get(CheckingRowKey(customerId), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(timeout)).Times(1);
  EXPECT_FALSE(smallbankTransaction.DepositChecking(mockSyncClient, "cust1", 18000));
}

TEST(DepositChecking, Success) {
  std::string name = "cust1";
  uint32_t customerId = 10;
  int timeout = 0;
  int deposit = 180000;
  int checkingBalance = 120;
  MockSyncClient mockSyncClient;
  SmallbankTransaction smallbankTransaction(DEPOSIT, "cust1", "", 0);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  proto::AccountRow accountRow;
  accountRow.set_name(name);
  accountRow.set_customer_id(customerId);
  std::string accountRowSerialized;
  accountRow.SerializeToString(&accountRowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(name), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(accountRowSerialized));
  proto::CheckingRow checkingRow;
  checkingRow.set_checking_balance(checkingBalance);
  checkingRow.set_customer_id(customerId);
  std::string checkingRowSerialized;
  checkingRow.SerializeToString(&checkingRowSerialized);
  EXPECT_CALL(mockSyncClient, Get(CheckingRowKey(customerId), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(checkingRowSerialized));
  proto::CheckingRow newCheckingRow;
  newCheckingRow.set_checking_balance(checkingBalance + deposit);
  newCheckingRow.set_customer_id(customerId);
  std::string newCheckingRowSerialized;
  newCheckingRow.SerializeToString(&newCheckingRowSerialized);
  EXPECT_CALL(mockSyncClient, Put(CheckingRowKey(customerId), newCheckingRowSerialized, timeout)).Times(1);
  EXPECT_CALL(mockSyncClient, Commit(timeout)).Times(1);
  EXPECT_TRUE(smallbankTransaction.DepositChecking(mockSyncClient, "cust1", deposit));
}

TEST(TransactSaving, ReadAccountFailure) {
  MockSyncClient mockSyncClient;
  SmallbankTransaction smallbankTransaction(TRANSACT, "cust1", "", 0);

  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey("cust1"), testing::_, 0)).WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(0)).Times(1);
  EXPECT_FALSE(smallbankTransaction.TransactSaving(mockSyncClient, "cust1", 18000));
}

TEST (TransactSaving, ReadSavingFailure) {
  MockSyncClient mockSyncClient;
  uint32_t customerId = 10;
  std::string name = "cust1";
  uint32_t timeout = 0;

  SmallbankTransaction smallbankTransaction(WRITE_CHECK, name, "", 0);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  proto::AccountRow accountRow;
  accountRow.set_name(name);
  accountRow.set_customer_id(customerId);
  std::string accountRowSerialized;
  accountRow.SerializeToString(&accountRowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(name), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(accountRowSerialized));
  EXPECT_CALL(mockSyncClient, Get(SavingRowKey(customerId), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(timeout)).Times(1);
  EXPECT_FALSE(smallbankTransaction.TransactSaving(mockSyncClient, name, 18000));
}

TEST(TransactSaving, ResultingBalanceNegative) {
  MockSyncClient mockSyncClient;
  uint32_t customerId = 10;
  std::string name = "cust1";
  uint32_t timeout = 0;
  uint32_t savingBalance = 150;

  SmallbankTransaction smallbankTransaction(WRITE_CHECK, name, "", 0);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  proto::AccountRow accountRow;
  accountRow.set_name(name);
  accountRow.set_customer_id(customerId);
  std::string accountRowSerialized;
  accountRow.SerializeToString(&accountRowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(name), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(accountRowSerialized));
  proto::SavingRow savingRow;
  savingRow.set_saving_balance(savingBalance);
  savingRow.set_customer_id(customerId);
  std::string savingRowSerialized;
  savingRow.SerializeToString(&savingRowSerialized);
  EXPECT_CALL(mockSyncClient, Get(SavingRowKey(customerId), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(savingRowSerialized));
  EXPECT_CALL(mockSyncClient, Abort(timeout)).Times(1);
  EXPECT_FALSE(smallbankTransaction.TransactSaving(mockSyncClient, name, -151));
}

TEST(TransactSaving, Success) {
  MockSyncClient mockSyncClient;
  uint32_t customerId = 10;
  std::string name = "cust1";
  uint32_t timeout = 0;
  uint32_t savingBalance = 150;
  int transact = -150;

  SmallbankTransaction smallbankTransaction(WRITE_CHECK, name, "", 0);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  proto::AccountRow accountRow;
  accountRow.set_name(name);
  accountRow.set_customer_id(customerId);
  std::string accountRowSerialized;
  accountRow.SerializeToString(&accountRowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(name), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(accountRowSerialized));
  proto::SavingRow savingRow;
  savingRow.set_saving_balance(savingBalance);
  savingRow.set_customer_id(customerId);
  std::string savingRowSerialized;
  savingRow.SerializeToString(&savingRowSerialized);
  EXPECT_CALL(mockSyncClient, Get(SavingRowKey(customerId), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(savingRowSerialized));
  EXPECT_CALL(mockSyncClient, Commit(timeout)).Times(1);
  proto::SavingRow newSavingRow;
  newSavingRow.set_saving_balance(savingBalance + transact);
  newSavingRow.set_customer_id(customerId);
  std::string newSavingRowSerialized;
  newSavingRow.SerializeToString(&newSavingRowSerialized);
  EXPECT_CALL(mockSyncClient, Put(SavingRowKey(customerId), newSavingRowSerialized, timeout)).Times(1);
  EXPECT_TRUE(smallbankTransaction.TransactSaving(mockSyncClient, name, transact));
}

TEST(Amalgamate, ReadAccount1Failure) {
  MockSyncClient mockSyncClient;
  SmallbankTransaction smallbankTransaction(AMALGAMATE, "cust1", "cust2", 0);
  std::string name1 = "cust1";
  std::string name2 = "cust2";
  uint32_t timeout = 0;
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);

  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(name1), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(0)).Times(1);
  EXPECT_FALSE(smallbankTransaction.Amalgamate(mockSyncClient, name1, name2));
}

TEST(Amalgamate, ReadAccount2Failure) {
  MockSyncClient mockSyncClient;
  SmallbankTransaction smallbankTransaction(AMALGAMATE, "cust1", "cust2", 0);
  std::string name1 = "cust1";
  std::string name2 = "cust2";
  uint32_t customerId1 = 100;
  uint32_t timeout = 0;
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);

  proto::AccountRow account1Row;
  account1Row.set_name(name1);
  account1Row.set_customer_id(customerId1);
  std::string account1RowSerialized;
  account1Row.SerializeToString(&account1RowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(name1), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(account1RowSerialized));
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(name2), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(0)).Times(1);
  EXPECT_FALSE(smallbankTransaction.Amalgamate(mockSyncClient, name1, name2));
}
TEST(Amalgamate, ReadChecking2Failure) {
  MockSyncClient mockSyncClient;
  SmallbankTransaction smallbankTransaction(AMALGAMATE, "cust1", "cust2", 0);
  std::string name1 = "cust1";
  std::string name2 = "cust2";
  uint32_t customerId1 = 100;
  uint32_t customerId2 = 200;
  uint32_t timeout = 0;
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);

  proto::AccountRow account1Row;
  account1Row.set_name(name1);
  account1Row.set_customer_id(customerId1);
  std::string account1RowSerialized;
  account1Row.SerializeToString(&account1RowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(name1), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(account1RowSerialized));
  proto::AccountRow account2Row;
  account2Row.set_name(name2);
  account2Row.set_customer_id(customerId2);
  std::string account2RowSerialized;
  account2Row.SerializeToString(&account2RowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(name2), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(account2RowSerialized));
  EXPECT_CALL(mockSyncClient, Get(CheckingRowKey(customerId2), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(timeout)).Times(1);
  EXPECT_FALSE(smallbankTransaction.Amalgamate(mockSyncClient, name1, name2));
}

TEST(Amalgamate, ReadChecking1Failure) {
  MockSyncClient mockSyncClient;
  SmallbankTransaction smallbankTransaction(AMALGAMATE, "cust1", "cust2", 0);
  std::string name1 = "cust1";
  std::string name2 = "cust2";
  uint32_t checking2Balance = 50;
  uint32_t customerId1 = 100;
  uint32_t customerId2 = 200;
  uint32_t timeout = 0;
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);

  proto::AccountRow account1Row;
  account1Row.set_name(name1);
  account1Row.set_customer_id(customerId1);
  std::string account1RowSerialized;
  account1Row.SerializeToString(&account1RowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(name1), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(account1RowSerialized));
  proto::AccountRow account2Row;
  account2Row.set_name(name2);
  account2Row.set_customer_id(customerId2);
  std::string account2RowSerialized;
  account2Row.SerializeToString(&account2RowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(name2), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(account2RowSerialized));
  proto::CheckingRow checking2Row;
  checking2Row.set_checking_balance(checking2Balance);
  checking2Row.set_customer_id(customerId2);
  std::string checking2RowSerialized;
  checking2Row.SerializeToString(&checking2RowSerialized);
  EXPECT_CALL(mockSyncClient, Get(CheckingRowKey(customerId2), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(checking2RowSerialized));
  EXPECT_CALL(mockSyncClient, Get(CheckingRowKey(customerId1), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(timeout)).Times(1);
  EXPECT_FALSE(smallbankTransaction.Amalgamate(mockSyncClient, name1, name2));
}

TEST(Amalgamate, ReadSaving1Failure) {
  MockSyncClient mockSyncClient;
  SmallbankTransaction smallbankTransaction(AMALGAMATE, "cust1", "cust2", 0);
  std::string name1 = "cust1";
  std::string name2 = "cust2";
  uint32_t checking1Balance = 15;
  uint32_t checking2Balance = 50;
  uint32_t customerId1 = 100;
  uint32_t customerId2 = 200;
  uint32_t timeout = 0;
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);

  proto::AccountRow account1Row;
  account1Row.set_name(name1);
  account1Row.set_customer_id(customerId1);
  std::string account1RowSerialized;
  account1Row.SerializeToString(&account1RowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(name1), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(account1RowSerialized));
  proto::AccountRow account2Row;
  account2Row.set_name(name2);
  account2Row.set_customer_id(customerId2);
  std::string account2RowSerialized;
  account2Row.SerializeToString(&account2RowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(name2), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(account2RowSerialized));
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
  EXPECT_CALL(mockSyncClient, Get(CheckingRowKey(customerId2), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(checking2RowSerialized));
  EXPECT_CALL(mockSyncClient, Get(CheckingRowKey(customerId1), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(checking1RowSerialized));
  EXPECT_CALL(mockSyncClient, Get(SavingRowKey(customerId1), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(timeout)).Times(1);
  EXPECT_FALSE(smallbankTransaction.Amalgamate(mockSyncClient, name1, name2));
}

TEST(Amalgamate, Success) {
  MockSyncClient mockSyncClient;
  SmallbankTransaction smallbankTransaction(AMALGAMATE, "cust1", "cust2", 0);
  std::string name1 = "cust1";
  std::string name2 = "cust2";
  uint32_t checking1Balance = 15;
  uint32_t checking2Balance = 50;
  uint32_t saving1Balance = 2002;
  uint32_t customerId1 = 100;
  uint32_t customerId2 = 200;
  uint32_t timeout = 0;
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);

  proto::AccountRow account1Row;
  account1Row.set_name(name1);
  account1Row.set_customer_id(customerId1);
  std::string account1RowSerialized;
  account1Row.SerializeToString(&account1RowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(name1), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(account1RowSerialized));
  proto::AccountRow account2Row;
  account2Row.set_name(name2);
  account2Row.set_customer_id(customerId2);
  std::string account2RowSerialized;
  account2Row.SerializeToString(&account2RowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(name2), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(account2RowSerialized));
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
  EXPECT_CALL(mockSyncClient, Get(CheckingRowKey(customerId2), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(checking2RowSerialized));
  EXPECT_CALL(mockSyncClient, Get(CheckingRowKey(customerId1), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(checking1RowSerialized));
  EXPECT_CALL(mockSyncClient, Get(SavingRowKey(customerId1), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(saving1RowSerialized));
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
  newChecking2Row.set_checking_balance(checking2Balance + checking1Balance + saving1Balance);
  newChecking2Row.set_customer_id(customerId2);
  std::string newChecking2RowSerialized;
  newChecking2Row.SerializeToString(&newChecking2RowSerialized);
  EXPECT_CALL(mockSyncClient, Put(CheckingRowKey(customerId1), newChecking1RowSerialized, timeout)).Times(1);
  EXPECT_CALL(mockSyncClient, Put(SavingRowKey(customerId1), newSaving1RowSerialized, timeout)).Times(1);
  EXPECT_CALL(mockSyncClient, Put(CheckingRowKey(customerId2), newChecking2RowSerialized, timeout)).Times(1);
  EXPECT_CALL(mockSyncClient, Commit(timeout)).Times(1);
  EXPECT_TRUE(smallbankTransaction.Amalgamate(mockSyncClient, name1, name2));
}

TEST(WriteCheck, ReadAccountFailure) {
  MockSyncClient mockSyncClient;
  SmallbankTransaction smallbankTransaction(WRITE_CHECK, "cust1", "", 0);

  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey("cust1"), testing::_, 0)).WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(0)).Times(1);
  EXPECT_FALSE(smallbankTransaction.WriteCheck(mockSyncClient, "cust1", 18000));
}

TEST(WriteCheck, ReadCheckingFailure) {
  MockSyncClient mockSyncClient;
  uint32_t customerId = 10;
  std::string name = "cust1";
  uint32_t timeout = 0;

  SmallbankTransaction smallbankTransaction(WRITE_CHECK, "cust1", "", 0);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  proto::AccountRow accountRow;
  accountRow.set_name(name);
  accountRow.set_customer_id(customerId);
  std::string accountRowSerialized;
  accountRow.SerializeToString(&accountRowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(name), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(accountRowSerialized));
  EXPECT_CALL(mockSyncClient, Get(CheckingRowKey(customerId), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(timeout)).Times(1);
  EXPECT_FALSE(smallbankTransaction.WriteCheck(mockSyncClient, name, 18000));
}
TEST(WriteCheck, ReadSavingFailure) {
  MockSyncClient mockSyncClient;
  uint32_t customerId = 10;
  std::string name = "cust1";
  uint32_t timeout = 0;
  uint32_t checkingBalance = 150;

  SmallbankTransaction smallbankTransaction(WRITE_CHECK, "cust1", "", 0);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  proto::AccountRow accountRow;
  accountRow.set_name(name);
  accountRow.set_customer_id(customerId);
  std::string accountRowSerialized;
  accountRow.SerializeToString(&accountRowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(name), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(accountRowSerialized));
  proto::CheckingRow checkingRow;
  checkingRow.set_checking_balance(checkingBalance);
  checkingRow.set_customer_id(customerId);
  std::string checkingRowSerialized;
  checkingRow.SerializeToString(&checkingRowSerialized);
  EXPECT_CALL(mockSyncClient, Get(CheckingRowKey(customerId), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(checkingRowSerialized));
  EXPECT_CALL(mockSyncClient, Get(SavingRowKey(customerId), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(timeout)).Times(1);
  EXPECT_FALSE(smallbankTransaction.WriteCheck(mockSyncClient, name, 18000));
}

TEST(WriteCheck, ExactlySufficientFunds) {
  MockSyncClient mockSyncClient;
  uint32_t customerId = 10;
  std::string name = "cust1";
  uint32_t timeout = 0;
  uint32_t checkingBalance = 150;
  uint32_t savingBalance = 200;
  uint32_t withdrawal = checkingBalance + savingBalance;

  SmallbankTransaction smallbankTransaction(WRITE_CHECK, "cust1", "", 0);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  proto::AccountRow accountRow;
  accountRow.set_name(name);
  accountRow.set_customer_id(customerId);
  std::string accountRowSerialized;
  accountRow.SerializeToString(&accountRowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(name), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(accountRowSerialized));
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
  EXPECT_CALL(mockSyncClient, Get(CheckingRowKey(customerId), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(checkingRowSerialized));
  EXPECT_CALL(mockSyncClient, Get(SavingRowKey(customerId), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(savingRowSerialized));
  proto::CheckingRow newCheckingRow;
  newCheckingRow.set_checking_balance(checkingBalance - withdrawal);
  newCheckingRow.set_customer_id(customerId);
  std::string newCheckingRowSerialized;
  newCheckingRow.SerializeToString(&newCheckingRowSerialized);
  EXPECT_CALL(mockSyncClient, Put(CheckingRowKey(customerId), newCheckingRowSerialized, timeout)).Times(1);
  EXPECT_CALL(mockSyncClient, Commit(timeout)).Times(1);
  EXPECT_TRUE(smallbankTransaction.WriteCheck(mockSyncClient, name, withdrawal));
}

TEST(WriteCheck, InsufficientFunds) {
  MockSyncClient mockSyncClient;
  uint32_t customerId = 10;
  std::string name = "cust1";
  uint32_t timeout = 0;
  uint32_t checkingBalance = 150;
  uint32_t savingBalance = 200;
  uint32_t withdrawal = checkingBalance + savingBalance + 1000;

  SmallbankTransaction smallbankTransaction(WRITE_CHECK, "cust1", "", 0);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  proto::AccountRow accountRow;
  accountRow.set_name(name);
  accountRow.set_customer_id(customerId);
  std::string accountRowSerialized;
  accountRow.SerializeToString(&accountRowSerialized);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(name), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(accountRowSerialized));
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
  EXPECT_CALL(mockSyncClient, Get(CheckingRowKey(customerId), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(checkingRowSerialized));
  EXPECT_CALL(mockSyncClient, Get(SavingRowKey(customerId), testing::_, timeout)).WillOnce(testing::SetArgReferee<1>(savingRowSerialized));
  proto::CheckingRow newCheckingRow;
  // contains penalty for overdrawing
  newCheckingRow.set_checking_balance(checkingBalance - (withdrawal + 1));
  newCheckingRow.set_customer_id(customerId);
  std::string newCheckingRowSerialized;
  newCheckingRow.SerializeToString(&newCheckingRowSerialized);
  EXPECT_CALL(mockSyncClient, Put(CheckingRowKey(customerId), newCheckingRowSerialized, timeout)).Times(1);
  EXPECT_CALL(mockSyncClient, Commit(timeout)).Times(1);
  EXPECT_TRUE(smallbankTransaction.WriteCheck(mockSyncClient, name, withdrawal));
}

TEST(InsertAccountRow, Basic) {
  SmallbankTransaction smallbankTransaction(BALANCE, "cust1","", 0);
  
  MockSyncClient mockSyncClient;
  proto::AccountRow mockAccountRow;
  mockAccountRow.set_name("cust1");
  mockAccountRow.set_customer_id(10);
  std::string row;
  mockAccountRow.SerializeToString(&row);
  EXPECT_CALL(mockSyncClient, Put(AccountRowKey("cust1"), row, 0)).Times(1);
  smallbankTransaction.InsertAccountRow(mockSyncClient, "cust1", 10);
}

TEST(InsertSavingRow, Basic) {
  SmallbankTransaction smallbankTransaction(BALANCE, "cust1", "", 0);
  
  MockSyncClient mockSyncClient;
  proto::SavingRow mockSavingRow;
  mockSavingRow.set_saving_balance(18000);
  mockSavingRow.set_customer_id(10);
  std::string row;
  mockSavingRow.SerializeToString(&row);
  EXPECT_CALL(mockSyncClient, Put(SavingRowKey(10), row, 0)).Times(1);
  smallbankTransaction.InsertSavingRow(mockSyncClient, 10, 18000);
}

TEST(InsertCheckingRow, Basic) {
  SmallbankTransaction smallbankTransaction(BALANCE, "cust1", "", 0);
  
  MockSyncClient mockSyncClient;
  proto::CheckingRow mockCheckingRow;
  mockCheckingRow.set_checking_balance(18000);
  mockCheckingRow.set_customer_id(10);
  std::string row;
  mockCheckingRow.SerializeToString(&row);
  EXPECT_CALL(mockSyncClient, Put(CheckingRowKey(10), row, 0)).Times(1);
  smallbankTransaction.InsertCheckingRow(mockSyncClient, 10, 18000);
}

TEST(ReadAccountRow, Basic) {
  SmallbankTransaction smallbankTransaction(BALANCE, "cust1", "", 0);
  
  MockSyncClient mockSyncClient;
  proto::AccountRow mockAccountRow;
  mockAccountRow.set_name("cust1");
  mockAccountRow.set_customer_id(10);
  std::string row;
  mockAccountRow.SerializeToString(&row);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey("cust1"), testing::_, 0)).WillOnce(testing::SetArgReferee<1>(row));

  proto::AccountRow accountRow;
  EXPECT_TRUE(smallbankTransaction.ReadAccountRow(mockSyncClient, "cust1", accountRow));
  EXPECT_EQ(accountRow.name(), "cust1");
  EXPECT_EQ(accountRow.customer_id(), 10);
}

TEST(ReadAccountRow, EmptyProto) {
  SmallbankTransaction smallbankTransaction(BALANCE, "cust1", "", 0);
  
  MockSyncClient mockSyncClient;
  std::string row;
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey("cust1"), testing::_, 0)).WillOnce(testing::SetArgReferee<1>(row));

  proto::AccountRow accountRow;
  EXPECT_FALSE(smallbankTransaction.ReadAccountRow(mockSyncClient, "cust1", accountRow));
}

TEST(ReadSavingRow, Basic) {
  SmallbankTransaction smallbankTransaction(BALANCE, "cust1", "", 0);
  
  MockSyncClient mockSyncClient;
  proto::SavingRow mockSavingRow;
  mockSavingRow.set_saving_balance(18000);
  mockSavingRow.set_customer_id(10);
  std::string row;
  mockSavingRow.SerializeToString(&row);
  EXPECT_CALL(mockSyncClient, Get(SavingRowKey(10), testing::_, 0)).WillOnce(testing::SetArgReferee<1>(row));

  proto::SavingRow savingRow;
  EXPECT_TRUE(smallbankTransaction.ReadSavingRow(mockSyncClient, 10, savingRow));
  EXPECT_EQ(savingRow.saving_balance(), 18000);
  EXPECT_EQ(savingRow.customer_id(), 10);
}

TEST(ReadSavingRow, EmptyProto) {
  SmallbankTransaction smallbankTransaction(BALANCE, "cust1", "", 0);
  
  MockSyncClient mockSyncClient;
  std::string row;
  EXPECT_CALL(mockSyncClient, Get(SavingRowKey(10), testing::_, 0)).WillOnce(testing::SetArgReferee<1>(row));

  proto::SavingRow savingRow;
  EXPECT_FALSE(smallbankTransaction.ReadSavingRow(mockSyncClient, 10, savingRow));
}

TEST(ReadCheckingRow, Basic) {
  SmallbankTransaction smallbankTransaction(BALANCE, "cust1", "", 0);
  
  MockSyncClient mockSyncClient;
  proto::CheckingRow mockCheckingRow;
  mockCheckingRow.set_checking_balance(18000);
  mockCheckingRow.set_customer_id(10);
  std::string row;
  mockCheckingRow.SerializeToString(&row);
  EXPECT_CALL(mockSyncClient, Get(CheckingRowKey(10), testing::_, 0)).WillOnce(testing::SetArgReferee<1>(row));

  proto::CheckingRow checkingRow;
  EXPECT_TRUE(smallbankTransaction.ReadCheckingRow(mockSyncClient, 10, checkingRow));
  EXPECT_EQ(checkingRow.checking_balance(), 18000);
  EXPECT_EQ(checkingRow.customer_id(), 10);
}

TEST(ReadCheckingRow, EmptyProto) {
  SmallbankTransaction smallbankTransaction(BALANCE, "cust1", "", 0);
  
  MockSyncClient mockSyncClient;
  std::string row;
  EXPECT_CALL(mockSyncClient, Get(CheckingRowKey(10), testing::_, 0)).WillOnce(testing::SetArgReferee<1>(row));

  proto::CheckingRow checkingRow;
  EXPECT_FALSE(smallbankTransaction.ReadCheckingRow(mockSyncClient, 10, checkingRow));
}

} // namespace smallbank
