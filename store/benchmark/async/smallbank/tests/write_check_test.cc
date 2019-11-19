//
// Created by Janice Chan on 10/12/19.
//
#include "store/benchmark/async/smallbank/smallbank_transaction.h"
#include "store/benchmark/async/smallbank/tests/smallbank_test_utils.h"
#include "store/benchmark/async/smallbank/utils.h"
#include "store/benchmark/async/smallbank/write_check.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace smallbank {
TEST(WriteCheck, ReadAccountFailure) {
  MockSyncClient mockSyncClient;
  WriteCheck smallbankTransaction("cust1", 18000, 0);

  EXPECT_CALL(mockSyncClient, Begin()).Times(1);

  EXPECT_CALL(mockSyncClient, Get(AccountRowKey("cust1"), testing::_, 0))
      .WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(0)).Times(1);
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), 1);
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
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), 1);
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
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), 1);
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
