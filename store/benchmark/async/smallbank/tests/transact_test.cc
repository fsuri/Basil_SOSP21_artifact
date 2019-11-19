//
// Created by Janice Chan on 10/12/19.
//
#include "store/benchmark/async/smallbank/smallbank_transaction.h"
#include "store/benchmark/async/smallbank/tests/smallbank_test_utils.h"
#include "store/benchmark/async/smallbank/transact.h"
#include "store/benchmark/async/smallbank/utils.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace smallbank {
TEST(TransactSaving, ReadAccountFailure) {
  MockSyncClient mockSyncClient;
  TransactSaving smallbankTransaction("cust1", 18000, 0);

  EXPECT_CALL(mockSyncClient, Begin()).Times(1);

  EXPECT_CALL(mockSyncClient, Get(AccountRowKey("cust1"), testing::_, 0))
      .WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(0)).Times(1);
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), 1);
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
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), 1);
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
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), 2);
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
}  // namespace smallbank
