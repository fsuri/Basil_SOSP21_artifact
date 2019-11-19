//
// Created by Janice Chan on 10/12/19.
//
#include "store/benchmark/async/smallbank/smallbank_transaction.h"
#include "store/benchmark/async/smallbank/tests/smallbank_test_utils.h"
#include "store/benchmark/async/smallbank/amalgamate.h"
#include "store/benchmark/async/smallbank/utils.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace smallbank {
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
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), 1);
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
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), 1);
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
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), 1);
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
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), 1);
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
  EXPECT_EQ(smallbankTransaction.Execute(mockSyncClient), 1);
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
}  // namespace smallbank
