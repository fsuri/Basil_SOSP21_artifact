//
// Created by Janice Chan on 10/12/19.
//
#include "store/benchmark/async/smallbank/smallbank_transaction.h"
#include "store/benchmark/async/smallbank/tests/smallbank_test_utils.h"
#include "store/benchmark/async/smallbank/deposit.h"
#include "store/benchmark/async/smallbank/utils.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace smallbank {
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
}  // namespace smallbank
