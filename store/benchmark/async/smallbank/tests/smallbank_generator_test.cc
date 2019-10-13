//
// Created by Janice Chan on 10/12/19.
//

#include "store/benchmark/async/smallbank/smallbank_generator.h"
#include <gtest/gtest.h>

TEST(RandomAString, Basic) {
	smallbank::SmallbankGenerator generator;
    std::mt19937 gen;
    size_t minSize = 8;
    size_t maxSize = 10;
    for (int i=0; i<50; i++) {
        std::string res = generator.RandomName(minSize, maxSize, gen);
        EXPECT_GE(res.size(), minSize);
        EXPECT_LE(res.size(), maxSize);
    }
}