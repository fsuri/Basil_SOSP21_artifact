//
// Created by Janice Chan on 10/12/19.
//
#include "store/benchmark/async/smallbank/smallbank_transaction.h"

#include <gtest/gtest.h>
#include "store/benchmark/async/smallbank/tests/fakeit.hpp"

namespace smallbank {
	// TEST(ReadCheckingRow, Basic) {
	// 	SmallbankTransaction smallbankTransaction(BALANCE, "cust1", "cust2", 0);
		

	// 	fakeit::Mock<SyncClient> syncClientMockWrapper;
	// 	std::cout<<"mustbehre"<<std::endl;
	// 	// fakeit::When(Method(syncClientMockWrapper,Get))
	// 	// 	.Do([](const std::string &key, std::string &value, uint32_t timeout) { 
	// 	// 	// 	std::cout<<"HERE"<<std::endl;
	// 	// 	// proto::CheckingRow mockReturnedCheckingRow;
	// 	// 	// mockReturnedCheckingRow.set_checking_balance(18000);
	// 	// 	// mockReturnedCheckingRow.set_customer_id(10);
	// 	// 	// mockReturnedCheckingRow.SerializeToString(&value);
	// 	// });
	// 		fakeit::When(Method(syncClientMockWrapper,Commit))
	// 		.AlwaysDo([](uint32_t timeout) -> int { 
	// 		// 	std::cout<<"HERE"<<std::endl;
	// 		// proto::CheckingRow mockReturnedCheckingRow;
	// 		// mockReturnedCheckingRow.set_checking_balance(18000);
	// 		// mockReturnedCheckingRow.set_customer_id(10);
	// 		// mockReturnedCheckingRow.SerializeToString(&value);
	// 			return 1;
	// 	});
	// 	std::cout<<"mehoy"<<std::endl;
	// 	// proto::CheckingRow checkingRow;
	// 	// std::cout<<"HEREaasdf"<<std::endl;
	// 	// smallbankTransaction.ReadCheckingRow(syncClientMockWrapper.get(), 10, checkingRow);
	// 	// EXPECT_EQ(checkingRow.checking_balance(), 18000);
	// 	// EXPECT_EQ(checkingRow.customer_id(), 10);
	// }

} // namespace smallbank