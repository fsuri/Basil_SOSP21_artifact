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

TEST(ReadCheckingRow, Basic) {
  SmallbankTransaction smallbankTransaction(BALANCE, "cust1", "cust2", 0);
  

  MockSyncClient mockSyncClient;
  std::string row = "";
  EXPECT_CALL(mockSyncClient, Get(CheckingRowKey(10), testing::_, 0)).WillOnce(testing::SetArgReferee<1>(row));

  proto::CheckingRow checkingRow;
  smallbankTransaction.ReadCheckingRow(mockSyncClient, 10, checkingRow);
  EXPECT_EQ(checkingRow.checking_balance(), 18000);
  EXPECT_EQ(checkingRow.customer_id(), 10);
}

} // namespace smallbank
