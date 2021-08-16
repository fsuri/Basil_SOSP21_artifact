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

#ifndef MORTY_TAPIR_SMALLBANK_GENERATOR_H
#define MORTY_TAPIR_SMALLBANK_GENERATOR_H
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
#include "store/benchmark/async/smallbank/smallbank-proto.pb.h"

namespace smallbank{
template<class T>
class Queue {
public:
    Queue(size_t maxSize) : maxSize(maxSize) { }

    void Push(const T &t) {
        std::unique_lock<std::mutex> lock(mtx);
        while (IsFull()) {
            cond.wait(lock);
        }
        q.push(t);
    }

    void Pop(T &t) {
        std::unique_lock<std::mutex> lock(mtx);
        while (IsEmpty()) {
            cond.wait(lock);
        }
        t = q.front();
        q.pop();
        cond.notify_one();
    }

    bool IsEmpty() {
        return q.size() == 0;
    }
private:
    bool IsFull() {
        return q.size() == maxSize;
    }

    std::queue<T> q;
    std::mutex mtx;
    std::condition_variable cond;
    size_t maxSize;
};

class SmallbankGenerator {

public:
    std::string RandomName(size_t x, size_t y, std::mt19937 &gen);
    uint32_t RandomBalance(uint32_t base, uint32_t deviation, std::mt19937 &gen);
    void GenerateTables(Queue<std::pair<std::string, std::string>> &q, Queue<std::string> &names, uint32_t num_customers, uint32_t min_name_length, uint32_t max_name_length, uint32_t base_balance, uint32_t balance_deviation);

private:
    std::set<std::string, std::greater<std::string>> customerNames;
    std::string RandomAString(size_t x, size_t y, std::mt19937 &gen);
};
} // namespace smallbank
#endif //MORTY_TAPIR_SMALLBANK_GENERATOR_H