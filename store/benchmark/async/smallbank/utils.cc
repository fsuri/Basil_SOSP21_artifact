#include "store/benchmark/async/smallbank/utils.h"
#include "store/benchmark/async/smallbank/smallbank-proto.pb.h"

namespace smallbank {

    std::string AccountRowKey(const std::string &name) {
        char keyC[5];
        keyC[0] = static_cast<char>(Tables::ACCOUNT);
        *reinterpret_cast<std::string *>(keyC + 1) = name;
        return std::string(keyC, sizeof(keyC));
    }

    std::string SavingRowKey(const uint32_t &customer_id) {
        char keyC[5];
        keyC[0] = static_cast<char>(Tables::SAVING);
        *reinterpret_cast<uint32_t *>(keyC + 1) = customer_id;
        return std::string(keyC, sizeof(keyC));
    }

    std::string CheckingRowKey(const uint32_t &customer_id) {
        char keyC[5];
        keyC[0] = static_cast<char>(Tables::CHECKING);
        *reinterpret_cast<uint32_t *>(keyC + 1) = customer_id;
        return std::string(keyC, sizeof(keyC));
    }
}
