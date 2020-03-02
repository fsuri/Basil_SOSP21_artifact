#include "store/common/frontend/sync_client.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace smallbank {
	class MockSyncClient : public SyncClient {
	 public:
	  MockSyncClient() : SyncClient(nullptr) {}
	  MOCK_METHOD(void, Begin, (), (override));
	  MOCK_METHOD(void, Get,
	              (const std::string &key, std::string &val, uint32_t timeout),
	              (override));
	  MOCK_METHOD(void, Put,
	              (const std::string &key, const std::string &val,
	               uint32_t timeout),
	              (override));
	  MOCK_METHOD(int, Commit, (uint32_t timeout), (override));
	  MOCK_METHOD(void, Abort, (uint32_t timeout), (override));
	};
}