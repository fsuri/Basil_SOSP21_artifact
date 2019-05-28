//
// Created by 王珏晓 on 4/3/19.
//

#ifndef TPC_C_EXTRACT_H
#define TPC_C_EXTRACT_H

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include "store/benchmark/async/tpcc/new_order.h"

std::string select_from(const std::string &table, const std::string &key, const std::string &target );

#endif //TPC_C_EXTRACT_H
