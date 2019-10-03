#include <condition_variable>
#include <mutex>
#include <queue>
#include <random>
#include <string>
#include <utility>
#include <numeric>
#include <algorithm>

#include <gflags/gflags.h>

#include "lib/io_utils.h"
#include "store/benchmark/async/smallbank/utils.h"
#include "store/benchmark/async/smallbank/smallbank-proto.pb.h"

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

const char ALPHA_NUMERIC[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";


std::string RandomAString(size_t x, size_t y, std::mt19937 &gen) {
    std::string s;
    size_t length = std::uniform_int_distribution<size_t>(x,  y)(gen);
    for (size_t i = 0; i < length; ++i) {
        int j = std::uniform_int_distribution<size_t>(0, sizeof(ALPHA_NUMERIC))(gen);
        s += ALPHA_NUMERIC[j];
    }
    return s;
}
std::set<std::string, std::greater<std::string>> customerNames;

std::string RandomName(size_t x, size_t y, std::mt19937 &gen) {
    std::string name = RandomAString(x, y, gen);
    while (customerNames.find(name) != customerNames.end()) {
        name = RandomAString(x, y, gen);
    }
    customerNames.insert(name);
    return name;
}


uint32_t RandomBalance(uint32_t base, uint32_t deviation, std::mt19937 &gen) {
    return std::uniform_int_distribution<uint32_t>(base - deviation, base + deviation)(gen);
}

void GenerateTables(Queue<std::pair<std::string, std::string>> &q, uint32_t num_customers) {
    std::mt19937 gen;
    smallbank::proto::AccountRow accountRow;
    std::string accountRowOut;
    smallbank::proto::SavingRow savingRow;
    std::string savingRowOut;
    smallbank::proto::CheckingRow checkingRow;
    std::string checkingRowOut;

for (uint32_t cId = 1; cId <= num_customers; cId++) {
        std::string customerName = RandomName(8, 16, gen);
        accountRow.set_customer_id(cId);
        accountRow.set_name(customerName);

        savingRow.set_customer_id(cId);
        savingRow.set_balance(RandomBalance(1000,50,gen));

        checkingRow.set_customer_id(cId);
        checkingRow.set_balance(RandomBalance(1000,50,gen));

        accountRow.SerializeToString(&accountRowOut);
        savingRow.SerializeToString(&savingRowOut);
        checkingRow.SerializeToString(&checkingRowOut);

        std::string accountRowKey = smallbank::AccountRowKey(customerName);
        std::string savingRowKey = smallbank::SavingRowKey(cId);
        std::string checkingRowKey = smallbank::CheckingRowKey(cId);
        q.Push(std::make_pair(accountRowKey, accountRowOut));
        q.Push(std::make_pair(savingRowKey, savingRowOut));
        q.Push(std::make_pair(checkingRowKey, checkingRowOut));
    }
}

DEFINE_uint32(num_customers, 18000, "Number of customers");
int main(int argc, char *argv[]) {
    gflags::SetUsageMessage(
            "generates a file containing key-value pairs of Smallbank table data\n");
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    Queue<std::pair<std::string, std::string>> q(2e9);
    uint32_t time = std::time(0);
    std::cerr << "Generating " << FLAGS_num_customers << " customers." << std::endl;
    GenerateTables(q, FLAGS_num_customers);
    std::pair<std::string, std::string> out;
    size_t count = 0;
    while (!q.IsEmpty()) {
        q.Pop(out);
        count += WriteBytesToStream(&std::cout, out.first);
        count += WriteBytesToStream(&std::cout, out.second);
    }
    std::cerr << "Wrote " << count / 1024 / 1024 << "MB." << std::endl;
    return 0;
}
