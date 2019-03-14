#ifndef NEW_ORDER_H
#define NEW_ORDER_H

#include "store/benchmark/async/tpcc/tpcc_transaction.h"

namespace tpcc {

class NewOrder : public TPCCTransaction {
 public:
  NewOrder(Client *client);
  virtual ~NewOrder();

  void ExecuteNextOperation();

};

}

#endif /* NEW_ORDER_H */
