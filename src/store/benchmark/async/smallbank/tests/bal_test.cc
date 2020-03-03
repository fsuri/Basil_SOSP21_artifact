//
// Created by Janice Chan on 10/12/19.
//
#include "store/benchmark/async/smallbank/smallbank_transaction.h"
#include "store/benchmark/async/smallbank/tests/smallbank_test_utils.h"
#include "store/benchmark/async/smallbank/bal.h"
#include "store/benchmark/async/smallbank/utils.h"

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
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), 1);
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
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), 1);
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
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), 1);
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
  EXPECT_CALL(mockSyncClient, Commit(timeout)).Times(1).WillOnce(testing::Return(1));
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), 1);
}
}  // namespace smallbank
