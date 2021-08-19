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

#include "store/benchmark/async/smallbank/smallbank_generator.h"
#include "store/benchmark/async/smallbank/smallbank-proto.pb.h"
#include <gtest/gtest.h>

namespace smallbank {
	TEST(RandomName, Basic) {
		smallbank::SmallbankGenerator generator;
	    std::mt19937 gen;
	    size_t minSize = 8;
	    size_t maxSize = 10;
	    for (int i=0; i<1000; i++) {
	        std::string res = generator.RandomName(minSize, maxSize, gen);
	        EXPECT_GE(res.size(), minSize);
	        EXPECT_LE(res.size(), maxSize);
	    }
	}

	TEST(RandomBalance, Basic) {
		smallbank::SmallbankGenerator generator;
	    std::mt19937 gen;
	    size_t base = 1000;
	    size_t deviation = 50;
	    for (int i=0; i<1000; i++) {
	        uint32_t res = generator.RandomBalance(base, deviation, gen);
	        EXPECT_GE(res, base-deviation);
	        EXPECT_LE(res, base+deviation);
	    }
	}

	TEST(GenerateTables, Basic) {
		uint32_t numCustomers = 500;
		uint32_t baseBalance = 1000;
		uint32_t balanceDeviation = 50;
		uint32_t minNameLength = 8;
		uint32_t maxNameLength = 16;
	 	smallbank::Queue<std::pair<std::string, std::string>> q(2e9);
	    smallbank::Queue<std::string> names(2e9);
	    smallbank::SmallbankGenerator generator;
	    generator.GenerateTables(q, names, numCustomers, minNameLength, maxNameLength, baseBalance, balanceDeviation);
		
		std::pair<std::string, std::string> qOut;
	    std::string namesOut;

	    // q alternates proto type every 3 elements:
	    // 0. AccountRow, 1. SavingRow, 2. CheckingRow
	    std::unordered_set <std::string> tableKeySet;
	    int tableType = 0;
	    while (!q.IsEmpty()) {
		    q.Pop(qOut);
		    // unique keys
		    EXPECT_EQ(tableKeySet.find(qOut.first), tableKeySet.end());
		    tableKeySet.insert(qOut.first);
		    if (tableType == 0) {
	    		smallbank::proto::AccountRow accountRow;
	    		EXPECT_TRUE(accountRow.ParseFromString(qOut.second));
	    		EXPECT_GE(accountRow.name().size(), 8);
	    		EXPECT_LE(accountRow.name().size(), 16);
		    }
		    if (tableType == 1) {
	    		smallbank::proto::SavingRow savingRow;
	    		EXPECT_TRUE(savingRow.ParseFromString(qOut.second));
	    		EXPECT_GE(savingRow.saving_balance(), 1000-50);
	    		EXPECT_LE(savingRow.saving_balance(), 1000+50);
		    }
		    if (tableType == 2) {
	    		smallbank::proto::CheckingRow checkingRow;
	    		EXPECT_TRUE(checkingRow.ParseFromString(qOut.second));
	    		EXPECT_GE(checkingRow.checking_balance(), 1000-50);
	    		EXPECT_LE(checkingRow.checking_balance(), 1000+50);
		    }
		    tableType = (tableType + 1) % 3;
		}

		std::unordered_set <std::string> nameSet; 
		while (!names.IsEmpty()) {
		    names.Pop(namesOut);
		    // unique names
		    EXPECT_EQ(nameSet.find(namesOut), nameSet.end());
		    nameSet.insert(namesOut);
		}
		EXPECT_EQ(tableKeySet.size(), numCustomers * 3);
	    EXPECT_EQ(nameSet.size(), numCustomers);
	}
} // namespace smallbank