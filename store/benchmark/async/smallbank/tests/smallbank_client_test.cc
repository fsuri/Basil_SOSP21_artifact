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
		SmallbankClient client(syncClientMock, transportMock, 0,0,0,0,0,0,0,false,0,0,0,0,0,0,0,0,"","");
		
		int totalKeys = 18000;
		int numHotspotKeys = 1000;
		double hotspotProbability = 0.9;
		std::vector<std::string> keys;
		for (int i = 0; i < totalKeys; i++) {
			keys.push_back(std::to_string(i));
		}
		client.SetCustomerKeys(keys);

		// expect ~90% keys to be hotspot
		int hotspotKeysFound = 0;
		int nonHotspotKeysFound = 0;
		for (int i=0; i< 30000; i++) {
			std::string key = client.GetCustomerKey(generator, keys, numHotspotKeys, totalKeys - numHotspotKeys, hotspotProbability);
			auto keysItr = std::find(keys.begin(), keys.end(), key); 
			EXPECT_NE(keysItr, keys.end());
			int keyIdx = keysItr - keys.begin();
			if (keyIdx < numHotspotKeys) {
				hotspotKeysFound += 1;
			} else {
				nonHotspotKeysFound += 1;
			}
		}
		EXPECT_GT(hotspotKeysFound, 0.8 * (hotspotKeysFound+nonHotspotKeysFound));
	}

	TEST(GetCustomerKeyPair, Basic) {
		fakeit::Mock<SyncClient> syncClientMockWrapper;
		SyncClient & syncClientMock = syncClientMockWrapper.get();
		fakeit::Mock<Transport> transportMockWrapper;
		Transport & transportMock = transportMockWrapper.get();
		std::mt19937 generator(0);
		SmallbankClient client(syncClientMock, transportMock, 0,0,0,0,0,0,0,false,0,0,0,0,0,0,0,0,"","");
		
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
		double hotspotProbability = 0.9;
		for (int i=0; i< 30000; i++) {
			bool firstKeyInHotspot = false;
			std::pair<std::string, std::string> keyPair = client.GetCustomerKeyPair(generator, keys, numHotspotKeys, totalKeys - numHotspotKeys, hotspotProbability);
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
		// expect ~90% hotspot
		EXPECT_GT(hotspotKeyPairsFound, 0.8 * (hotspotKeyPairsFound+nonHotspotKeyPairsFound));
	}

	TEST(GetNextTransaction, EvenSplit) {
		fakeit::Mock<SyncClient> syncClientMockWrapper;
		SyncClient & syncClientMock = syncClientMockWrapper.get();
		fakeit::Mock<Transport> transportMockWrapper;
		Transport & transportMock = transportMockWrapper.get();
		std::mt19937 generator(0);
		int totalKeys = 18000;
		int numHotspotKeys = 1000;
		SmallbankClient client(syncClientMock, transportMock, 0,0,0,0,0,0,0,false,0,20,20,20,20,numHotspotKeys,totalKeys-numHotspotKeys,0.9, "","");
		
		std::vector<std::string> keys;
		for (int i = 0; i < totalKeys; i++) {
			keys.push_back(std::to_string(i));
		}
		client.SetCustomerKeys(keys);

		int typeOccurrences[5]={0};
		int totalOccurrences = 30000;
		for (int i = 0; i < totalOccurrences; i++) {
			typeOccurrences[((SmallbankTransaction* )client.GetNextTransaction())->GetTransactionType()] += 1;
		}
		// should be ~0.2 each
		for (int i = 0; i < 5; i++) {
			EXPECT_GT(typeOccurrences[i], 0.1*totalOccurrences);
			EXPECT_LT(typeOccurrences[i], 0.3*totalOccurrences);
		}
	}

	TEST(GetNextTransaction, AllBalance) {
		fakeit::Mock<SyncClient> syncClientMockWrapper;
		SyncClient & syncClientMock = syncClientMockWrapper.get();
		fakeit::Mock<Transport> transportMockWrapper;
		Transport & transportMock = transportMockWrapper.get();
		std::mt19937 generator(0);
		int totalKeys = 18000;
		int numHotspotKeys = 1000;
		SmallbankClient client(syncClientMock, transportMock, 0,0,0,0,0,0,0,false,0,100,0,0,0,numHotspotKeys,totalKeys-numHotspotKeys,0.9,"","");
		
		std::vector<std::string> keys;
		for (int i = 0; i < totalKeys; i++) {
			keys.push_back(std::to_string(i));
		}
		client.SetCustomerKeys(keys);

		int typeOccurrences[5]={0};
		int totalOccurrences = 30000;
		for (int i = 0; i < totalOccurrences; i++) {
			typeOccurrences[((SmallbankTransaction*)client.GetNextTransaction())->GetTransactionType()] += 1;
		}
		for (int j = 0; j < 5; j++) {
			std::cout<<typeOccurrences[j]<<std::endl;
		}
		for (int i = 0; i < 5; i++) {
			if (i == 0) {
				EXPECT_EQ(typeOccurrences[i], totalOccurrences);
			} else {
				EXPECT_EQ(typeOccurrences[i], 0);
			}
		}
	}
	TEST(GetNextTransaction, AllDeposit) {
		fakeit::Mock<SyncClient> syncClientMockWrapper;
		SyncClient & syncClientMock = syncClientMockWrapper.get();
		fakeit::Mock<Transport> transportMockWrapper;
		Transport & transportMock = transportMockWrapper.get();
		std::mt19937 generator(0);
		int totalKeys = 18000;
		int numHotspotKeys = 1000;
		SmallbankClient client(syncClientMock, transportMock, 0,0,0,0,0,0,0,false,0,0,100,0,0,numHotspotKeys,totalKeys-numHotspotKeys,0.9,"","");
		
		std::vector<std::string> keys;
		for (int i = 0; i < totalKeys; i++) {
			keys.push_back(std::to_string(i));
		}
		client.SetCustomerKeys(keys);

		int typeOccurrences[5]={0};
		int totalOccurrences = 30000;
		for (int i = 0; i < totalOccurrences; i++) {
			typeOccurrences[((SmallbankTransaction* )client.GetNextTransaction())->GetTransactionType()] += 1;
		}
		for (int i = 0; i < 5; i++) {
			if (i == 1) {
				EXPECT_EQ(typeOccurrences[i], totalOccurrences);
			} else {
				EXPECT_EQ(typeOccurrences[i], 0);
			}
		}
	}
	TEST(GetNextTransaction, AllTransact) {
		fakeit::Mock<SyncClient> syncClientMockWrapper;
		SyncClient & syncClientMock = syncClientMockWrapper.get();
		fakeit::Mock<Transport> transportMockWrapper;
		Transport & transportMock = transportMockWrapper.get();
		std::mt19937 generator(0);
		int totalKeys = 18000;
		int numHotspotKeys = 1000;
		SmallbankClient client(syncClientMock, transportMock, 0,0,0,0,0,0,0,false,0,0,0,100,0,numHotspotKeys,totalKeys-numHotspotKeys,0.9,"","");
		
		std::vector<std::string> keys;
		for (int i = 0; i < totalKeys; i++) {
			keys.push_back(std::to_string(i));
		}
		client.SetCustomerKeys(keys);

		int typeOccurrences[5]={0};
		int totalOccurrences = 30000;
		for (int i = 0; i < totalOccurrences; i++) {
			typeOccurrences[((SmallbankTransaction* )client.GetNextTransaction())->GetTransactionType()] += 1;
		}
		for (int i = 0; i < 5; i++) {
			if (i == 2) {
				EXPECT_EQ(typeOccurrences[i], totalOccurrences);
			} else {
				EXPECT_EQ(typeOccurrences[i], 0);
			}
		}
	}
	TEST(GetNextTransaction, AllAmalgamate) {
		fakeit::Mock<SyncClient> syncClientMockWrapper;
		SyncClient & syncClientMock = syncClientMockWrapper.get();
		fakeit::Mock<Transport> transportMockWrapper;
		Transport & transportMock = transportMockWrapper.get();
		std::mt19937 generator(0);
		int totalKeys = 18000;
		int numHotspotKeys = 1000;
		SmallbankClient client(syncClientMock, transportMock, 0,0,0,0,0,0,0,false,0,0,0,0,100,numHotspotKeys,totalKeys-numHotspotKeys,0.9,"","");
		
		std::vector<std::string> keys;
		for (int i = 0; i < totalKeys; i++) {
			keys.push_back(std::to_string(i));
		}
		client.SetCustomerKeys(keys);

		int typeOccurrences[5]={0};
		int totalOccurrences = 30000;
		for (int i = 0; i < totalOccurrences; i++) {
			typeOccurrences[((SmallbankTransaction* )client.GetNextTransaction())->GetTransactionType()] += 1;
		}
		for (int i = 0; i < 5; i++) {
			if (i == 3) {
				EXPECT_EQ(typeOccurrences[i], totalOccurrences);
			} else {
				EXPECT_EQ(typeOccurrences[i], 0);
			}
		}
	}

	TEST(GetNextTransaction, AllWriteCheck) {
		fakeit::Mock<SyncClient> syncClientMockWrapper;
		SyncClient & syncClientMock = syncClientMockWrapper.get();
		fakeit::Mock<Transport> transportMockWrapper;
		Transport & transportMock = transportMockWrapper.get();
		std::mt19937 generator(0);
		int totalKeys = 18000;
		int numHotspotKeys = 1000;
		SmallbankClient client(syncClientMock, transportMock, 0,0,0,0,0,0,0,false,0,0,0,0,0,numHotspotKeys,totalKeys-numHotspotKeys,0.9,"","");
		
		std::vector<std::string> keys;
		for (int i = 0; i < totalKeys; i++) {
			keys.push_back(std::to_string(i));
		}
		client.SetCustomerKeys(keys);

		int typeOccurrences[5]={0};
		int totalOccurrences = 30000;
		for (int i = 0; i < totalOccurrences; i++) {
			typeOccurrences[((SmallbankTransaction* )client.GetNextTransaction())->GetTransactionType()] += 1;
		}
		for (int i = 0; i < 5; i++) {
			if (i == 4) {
				EXPECT_EQ(typeOccurrences[i], totalOccurrences);
			} else {
				EXPECT_EQ(typeOccurrences[i], 0);
			}
		}
	}

	TEST(GetNextTransaction, OnlyTwoTypes) {
		fakeit::Mock<SyncClient> syncClientMockWrapper;
		SyncClient & syncClientMock = syncClientMockWrapper.get();
		fakeit::Mock<Transport> transportMockWrapper;
		Transport & transportMock = transportMockWrapper.get();
		std::mt19937 generator(0);
		int totalKeys = 18000;
		int numHotspotKeys = 1000;
		SmallbankClient client(syncClientMock, transportMock, 0,0,0,0,0,0,0,false,0,0,0,50,0,numHotspotKeys,totalKeys-numHotspotKeys,0.9,"","");
		
		std::vector<std::string> keys;
		for (int i = 0; i < totalKeys; i++) {
			keys.push_back(std::to_string(i));
		}
		client.SetCustomerKeys(keys);

		int typeOccurrences[5]={0};
		int totalOccurrences = 30000;
		for (int i = 0; i < totalOccurrences; i++) {
			typeOccurrences[((SmallbankTransaction* )client.GetNextTransaction())->GetTransactionType()] += 1;
		}
		// should be ~0.5 for types 2, 4
		for (int i = 0; i < 5; i++) {
			if (i == 4 || i == 2) {
				EXPECT_GT(typeOccurrences[i], 0.35*totalOccurrences);
				EXPECT_LT(typeOccurrences[i], 0.65*totalOccurrences);
			} else {
				EXPECT_EQ(typeOccurrences[i], 0);
			}
		}
	}
	
} // namespace smallbank