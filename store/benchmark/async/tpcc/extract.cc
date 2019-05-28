#include "extract.h"
#include "store/benchmark/async/tpcc/new_order.h"

namespace tpcc {

    std::string select_from(const std::string &table, const std::string &key, const std::string &target ) {

    std::string str = Get(table+key);
    size_t start = str.find(target);
    
    if (start == std::string::npos) {
        return "";
    } else {
        start += target.length() + 1;
        size_t end = start + 1;
        for (; end < str.length() && str[end] != ','; ++end);
        return str.substr(start, end - start);
    }

}

}