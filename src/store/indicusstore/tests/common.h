#ifndef INDICUS_TESTS_COMMON_H
#define INDICUS_TESTS_COMMON_H

#include <gmock/gmock.h>

#include <map>
#include <set>
#include <sstream>

#include "store/common/common-proto.pb.h"
#include "store/common/timestamp.h"
#include "store/indicusstore/indicus-proto.pb.h"

namespace indicusstore {

void GenerateTestConfig(int g, int f, std::stringstream &ss);

void PopulateTransaction(const std::map<std::string, Timestamp> &readSet,
    const std::map<std::string, std::string> &writeSet, const Timestamp &ts,
    const std::set<int> &involvedGroups, proto::Transaction &txn);

} // namespace indicusstore

#endif /* INDICUS_TESTS_COMMON_H */
