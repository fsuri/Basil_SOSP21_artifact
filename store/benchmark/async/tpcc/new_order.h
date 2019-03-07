#ifndef NEW_ORDER_H
#define NEW_ORDER_H

#include "store/benchmark/async/tpcc/tpcc_transaction.h"

namespace tpcc {

class NewOrder : public TPCCTransaction {
 public:
  NewOrder();
  virtual ~NewOrder();

  void ExecuteNextOperation(Client *client);

};

}

#endif /* NEW_ORDER_H */
