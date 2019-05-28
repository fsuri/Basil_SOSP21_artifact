#ifndef NEW_ORDER_H
#define NEW_ORDER_H

#include <string>
#include <unordered_map>
#include <vector>

#include "store/benchmark/async/tpcc/random_generator.h"
#include "store/benchmark/async/tpcc/tpcc_transaction.h"

namespace tpcc {

class NewOrder : public TPCCTransaction {
 public:
  NewOrder();
  virtual ~NewOrder();

  Operation GetNextOperation(size_t opCount,
      const std::map<std::string, std::string> &readValues);

 private:
  RandomGenerator* generator_;
  std::string last_query_;
  std::unordered_map<std::string, std::vector<std::string>> lists_;
  std::unordered_map<std::string, std::string> params_;
};

}

#endif /* NEW_ORDER_H */
