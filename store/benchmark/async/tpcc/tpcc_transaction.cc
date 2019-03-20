#include "store/benchmark/async/tpcc/tpcc_transaction.h"

namespace tpcc {

TPCCTransaction::TPCCTransaction(Client *client) : AsyncTransaction(0, client) {
}

TPCCTransaction::~TPCCTransaction() {
}

} // namespace tpcc
