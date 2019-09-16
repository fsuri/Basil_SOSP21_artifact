#include "store/common/frontend/client.h"
#include "store/benchmark/async/smallbank/database.pb.h"

namespace smallbank {
    class Benchmark {
    public:
        Benchmark(Client &client);

    private:
        Client &client_;

        AccountRow ReadAccountRow(const std::string &key);

        SavingRow ReadSavingRow(const std::string &key);

        CheckingRow ReadCheckingRow(const std::string &key);

        void InsertAccountRow(const std::string &key, const std::string &name);

        void InsertSavingRow(const std::string &key, const uint32_t &balance);

        void InsertCheckingRow(const std::string &key, const uint32_t &balance);

        void UpdateSavingRow(const std::string &key, const uint32_t &balance);

        void UpdateCheckingRow(const std::string &key, const uint32_t &balance);
    };
}  // namespace smallbank
