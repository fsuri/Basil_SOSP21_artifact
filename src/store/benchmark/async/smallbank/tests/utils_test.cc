/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fs435@cornell.edu>
 *                Matthew Burke <matthelb@cs.cornell.edu>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************************/
#include "store/benchmark/async/smallbank/tests/smallbank_test_utils.h"
#include "store/benchmark/async/smallbank/utils.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace smallbank {
TEST(InsertAccountRow, Basic) {
  uint32_t timeout = 0;
  std::string cust = "cust1";
  uint32_t customerId = 10;

  MockSyncClient mockSyncClient;
  proto::AccountRow mockAccountRow;
  mockAccountRow.set_name(cust);
  mockAccountRow.set_customer_id(customerId);
  std::string row;

  mockAccountRow.SerializeToString(&row);
  EXPECT_CALL(mockSyncClient, Put(AccountRowKey(cust), row, timeout)).Times(1);
  InsertAccountRow(mockSyncClient, cust, customerId, timeout);
}

TEST(InsertSavingRow, Basic) {
  uint32_t timeout = 0;
  uint32_t balance = 100;
  uint32_t customerId = 10;

  MockSyncClient mockSyncClient;
  proto::SavingRow mockSavingRow;
  mockSavingRow.set_saving_balance(balance);
  mockSavingRow.set_customer_id(customerId);
  std::string row;
  mockSavingRow.SerializeToString(&row);
  EXPECT_CALL(mockSyncClient, Put(SavingRowKey(10), row, timeout)).Times(1);
  InsertSavingRow(mockSyncClient, customerId, balance, timeout);
}

TEST(InsertCheckingRow, Basic) {
  uint32_t timeout = 0;
  uint32_t balance = 100;
  uint32_t customerId = 10;

  MockSyncClient mockSyncClient;
  proto::CheckingRow mockCheckingRow;
  mockCheckingRow.set_checking_balance(balance);
  mockCheckingRow.set_customer_id(customerId);
  std::string row;
  mockCheckingRow.SerializeToString(&row);
  EXPECT_CALL(mockSyncClient, Put(CheckingRowKey(customerId), row, timeout))
      .Times(1);
  InsertCheckingRow(mockSyncClient, customerId, balance, timeout);
}

TEST(ReadAccountRow, Basic) {
  uint32_t timeout = 0;
  std::string cust = "cust1";
  uint32_t customerId = 10;

  MockSyncClient mockSyncClient;
  proto::AccountRow mockAccountRow;
  mockAccountRow.set_name(cust);
  mockAccountRow.set_customer_id(customerId);
  std::string row;
  mockAccountRow.SerializeToString(&row);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(cust), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(row));

  proto::AccountRow accountRow;
  EXPECT_TRUE(ReadAccountRow(mockSyncClient, cust, accountRow, timeout));
  EXPECT_EQ(accountRow.name(), cust);
  EXPECT_EQ(accountRow.customer_id(), customerId);
}

TEST(ReadAccountRow, EmptyProto) {
  uint32_t timeout = 0;
  std::string cust = "cust1";

  MockSyncClient mockSyncClient;
  std::string row;
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey(cust), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(row));

  proto::AccountRow accountRow;
  EXPECT_FALSE(ReadAccountRow(mockSyncClient, cust, accountRow, timeout));
}

TEST(ReadSavingRow, Basic) {
  uint32_t balance = 18000;
  uint32_t customerId = 10;
  uint32_t timeout = 0;

  MockSyncClient mockSyncClient;
  proto::SavingRow mockSavingRow;
  mockSavingRow.set_saving_balance(balance);
  mockSavingRow.set_customer_id(customerId);
  std::string row;
  mockSavingRow.SerializeToString(&row);
  EXPECT_CALL(mockSyncClient,
              Get(SavingRowKey(customerId), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(row));

  proto::SavingRow savingRow;
  EXPECT_TRUE(ReadSavingRow(mockSyncClient, customerId, savingRow, timeout));
  EXPECT_EQ(savingRow.saving_balance(), balance);
  EXPECT_EQ(savingRow.customer_id(), customerId);
}

TEST(ReadSavingRow, EmptyProto) {
  uint32_t customerId = 10;
  uint32_t timeout = 0;

  MockSyncClient mockSyncClient;
  std::string row;
  EXPECT_CALL(mockSyncClient,
              Get(SavingRowKey(customerId), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(row));

  proto::SavingRow savingRow;
  EXPECT_FALSE(ReadSavingRow(mockSyncClient, customerId, savingRow, timeout));
}

TEST(ReadCheckingRow, Basic) {
  uint32_t customerId = 10;
  uint32_t timeout = 0;
  uint32_t balance = 18000;

  MockSyncClient mockSyncClient;
  proto::CheckingRow mockCheckingRow;
  mockCheckingRow.set_checking_balance(balance);
  mockCheckingRow.set_customer_id(customerId);
  std::string row;
  mockCheckingRow.SerializeToString(&row);
  EXPECT_CALL(mockSyncClient,
              Get(CheckingRowKey(customerId), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(row));

  proto::CheckingRow checkingRow;
  EXPECT_TRUE(
      ReadCheckingRow(mockSyncClient, customerId, checkingRow, timeout));
  EXPECT_EQ(checkingRow.checking_balance(), balance);
  EXPECT_EQ(checkingRow.customer_id(), customerId);
}

TEST(ReadCheckingRow, EmptyProto) {
  uint32_t customerId = 10;
  uint32_t timeout = 0;

  MockSyncClient mockSyncClient;
  std::string row;
  EXPECT_CALL(mockSyncClient,
              Get(CheckingRowKey(customerId), testing::_, timeout))
      .WillOnce(testing::SetArgReferee<1>(row));

  proto::CheckingRow checkingRow;
  EXPECT_FALSE(
      ReadCheckingRow(mockSyncClient, customerId, checkingRow, timeout));
}
}