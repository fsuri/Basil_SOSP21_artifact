#ifndef SMALLBANK_UTILS_H
#define SMALLBANK_UTILS_H

#include "store/benchmark/async/smallbank/smallbank-proto.pb.h"

namespace smallbank {

    std::string AccountRowKey(const std::string name);

    std::string SavingRowKey(const uint32_t customer_id);

    std::string CheckingRowKey(const uint32_t customer_id);


} // namespace smallbank

#endif /* SMALLBANK_UTILS_H */
