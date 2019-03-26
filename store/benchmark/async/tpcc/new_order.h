#ifndef NEW_ORDER_H
#define NEW_ORDER_H

#include "store/benchmark/async/tpcc/tpcc_transaction.h"

namespace tpcc {

class NewOrder : public TPCCTransaction {
 public:
  NewOrder();
  virtual ~NewOrder();

  Operation GetNextOperation(size_t opCount,
      const std::map<std::string, std::string> &readValues);

};

}

#endif /* NEW_ORDER_H */
