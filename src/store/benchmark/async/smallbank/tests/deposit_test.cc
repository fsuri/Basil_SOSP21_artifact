/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fsp@cs.cornell.edu>
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
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), 1);
}

TEST(DepositChecking, ReadAccountFailure) {
  MockSyncClient mockSyncClient;
  DepositChecking smallbankTransaction("cust1", 18000, 0);
  EXPECT_CALL(mockSyncClient, Begin()).Times(1);
  EXPECT_CALL(mockSyncClient, Get(AccountRowKey("cust1"), testing::_, 0))
      .WillOnce(testing::SetArgReferee<1>(""));
  EXPECT_CALL(mockSyncClient, Abort(0)).Times(1);
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), 1);
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
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), 1);
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
  EXPECT_CALL(mockSyncClient, Commit(timeout)).Times(1).WillOnce(testing::Return(1));
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), 1);
}
}  // namespace smallbank
