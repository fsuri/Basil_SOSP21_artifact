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
#include <condition_variable>
#include <mutex>
#include <queue>
#include <random>
#include <string>
#include <utility>
#include <numeric>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <gflags/gflags.h>

#include "lib/io_utils.h"
#include "store/benchmark/async/smallbank/utils.h"
#include "store/benchmark/async/smallbank/smallbank_generator.h"
#include "store/benchmark/async/smallbank/smallbank-proto.pb.h"

namespace smallbank {

const char ALPHA_NUMERIC[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

std::string SmallbankGenerator::RandomAString(size_t x, size_t y, std::mt19937 &gen) {
    std::string s;
    size_t length = std::uniform_int_distribution<size_t>(x,  y)(gen);
    for (size_t i = 0; i < length; ++i) {
        int j = std::uniform_int_distribution<size_t>(0, sizeof(ALPHA_NUMERIC))(gen);
        s += ALPHA_NUMERIC[j];
    }
    return s;
}

std::string SmallbankGenerator::RandomName(size_t x, size_t y, std::mt19937 &gen) {
    std::string name = RandomAString(x, y, gen);
    while (customerNames.find(name) != customerNames.end()) {
        name = RandomAString(x, y, gen);
    }
    customerNames.insert(name);
    return name;
}

uint32_t SmallbankGenerator::RandomBalance(uint32_t base, uint32_t deviation, std::mt19937 &gen) {
    return std::uniform_int_distribution<uint32_t>(base - deviation, base + deviation)(gen);
}

void SmallbankGenerator::GenerateTables(smallbank::Queue<std::pair<std::string, std::string>> &q, smallbank::Queue<std::string> &names, uint32_t num_customers, uint32_t min_name_length, uint32_t max_name_length, uint32_t base_balance, uint32_t balance_deviation) {
    std::mt19937 gen;
    smallbank::proto::AccountRow accountRow;
    std::string accountRowOut;
    smallbank::proto::SavingRow savingRow;
    std::string savingRowOut;
    smallbank::proto::CheckingRow checkingRow;
    std::string checkingRowOut;

    // TODO test and ensure values are within bounds
    for (uint32_t cId = 1; cId <= num_customers; cId++) {
        std::string customerName = RandomName(min_name_length, max_name_length, gen);
        accountRow.set_customer_id(cId);
        accountRow.set_name(customerName);

        savingRow.set_customer_id(cId);
        uint32_t savingBalance = RandomBalance(base_balance, balance_deviation, gen);
        savingRow.set_saving_balance(savingBalance);

        checkingRow.set_customer_id(cId);
        uint32_t checkingBalance = RandomBalance(base_balance, balance_deviation, gen);
        checkingRow.set_checking_balance(checkingBalance);

        accountRow.SerializeToString(&accountRowOut);
        savingRow.SerializeToString(&savingRowOut);
        checkingRow.SerializeToString(&checkingRowOut);

        std::string accountRowKey = smallbank::AccountRowKey(customerName);
        std::string savingRowKey = smallbank::SavingRowKey(cId);
        std::string checkingRowKey = smallbank::CheckingRowKey(cId);
        q.Push(std::make_pair(accountRowKey, accountRowOut));
        q.Push(std::make_pair(savingRowKey, savingRowOut));
        q.Push(std::make_pair(checkingRowKey, checkingRowOut));

        names.Push(customerName);
    }
}
}