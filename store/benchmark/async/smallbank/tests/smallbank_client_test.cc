//
// Created by Janice Chan on 10/12/19.
//
#include "store/common/frontend/client.h"
#include "store/common/frontend/sync_client.h"
#include "lib/tcptransport.h"

#include "store/benchmark/async/smallbank/smallbank_client.h"
#include "store/benchmark/async/smallbank/smallbank-proto.pb.h"

#include <gtest/gtest.h>
#include "store/benchmark/async/smallbank/tests/fakeit.hpp"
namespace smallbank {
	TEST(GetCustomerKey, Basic) {
		fakeit::Mock<SyncClient> syncClientMockWrapper;
		SyncClient & syncClientMock = syncClientMockWrapper.get();
		fakeit::Mock<Transport> transportMockWrapper;
		Transport & transportMock = transportMockWrapper.get();
		std::mt19937 generator(0);
		SmallbankClient client(syncClientMock, transportMock, 0,0,0,0,0,0,0,false,0,0,0,0,0,0,0,"","");
		
		int totalKeys = 18000;
		int numHotspotKeys = 1000;
		std::vector<std::string> keys;
		for (int i = 0; i < totalKeys; i++) {
			keys.push_back(std::to_string(i));
		}
		client.SetCustomerKeys(keys);

		// expect many more non hotspot keys found (since only 1000 of 18000 are hotspot keys)
		// and nonzero number of hotspot keys found
		int hotspotKeysFound = 0;
		int nonHotspotKeysFound = 0;
		for (int i=0; i< 30000; i++) {
			std::string key = client.GetCustomerKey(generator, keys, numHotspotKeys, totalKeys - numHotspotKeys);
			auto keysItr = std::find(keys.begin(), keys.end(), key); 
			EXPECT_NE(keysItr, keys.end());
			int keyIdx = keysItr - keys.begin();
			if (keyIdx < numHotspotKeys) {
				hotspotKeysFound += 1;
			} else {
				nonHotspotKeysFound += 1;
			}
		}
		EXPECT_GT(0.15 * nonHotspotKeysFound, hotspotKeysFound);
		EXPECT_GT(hotspotKeysFound, 0);
	}

	TEST(GetCustomerKeyPair, Basic) {
		fakeit::Mock<SyncClient> syncClientMockWrapper;
		SyncClient & syncClientMock = syncClientMockWrapper.get();
		fakeit::Mock<Transport> transportMockWrapper;
		Transport & transportMock = transportMockWrapper.get();
		std::mt19937 generator(0);
		SmallbankClient client(syncClientMock, transportMock, 0,0,0,0,0,0,0,false,0,0,0,0,0,0,0,"","");
		
		int totalKeys = 18000;
		int numHotspotKeys = 1000;
		std::vector<std::string> keys;
		for (int i = 0; i < totalKeys; i++) {
			keys.push_back(std::to_string(i));
		}
		client.SetCustomerKeys(keys);

		// pairs: both in or both not in hotspot
		int hotspotKeyPairsFound = 0;
		int nonHotspotKeyPairsFound = 0;
		for (int i=0; i< 30000; i++) {
			bool firstKeyInHotspot = false;
			std::pair<std::string, std::string> keyPair = client.GetCustomerKeyPair(generator, keys, numHotspotKeys, totalKeys - numHotspotKeys);
			auto keysItr = std::find(keys.begin(), keys.end(), keyPair.first); 
			EXPECT_NE(keysItr, keys.end());
			if (keysItr - keys.begin() < numHotspotKeys) {
				firstKeyInHotspot = true;
			} 
			keysItr = std::find(keys.begin(), keys.end(), keyPair.second); 
			EXPECT_NE(keysItr, keys.end());
			EXPECT_NE(keyPair.second, keyPair.first);
			int key2Idx = keysItr - keys.begin();
			EXPECT_TRUE(firstKeyInHotspot ? key2Idx < numHotspotKeys : key2Idx >= numHotspotKeys);
			if (firstKeyInHotspot) {
				hotspotKeyPairsFound += 1;
			} else {
				nonHotspotKeyPairsFound += 1;
			}
		}
		// expect many more non hotspot keys found (since only 1000 of 18000 are hotspot keys)
		// and nonzero number of hotspot keys found
		EXPECT_GT(0.15 * nonHotspotKeyPairsFound, hotspotKeyPairsFound);
		EXPECT_GT(hotspotKeyPairsFound, 0);
	}
	
} // namespace smallbank