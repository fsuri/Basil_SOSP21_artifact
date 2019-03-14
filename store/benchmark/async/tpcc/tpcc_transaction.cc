#include "store/benchmark/async/tpcc/tpcc_transaction.h"

namespace tpcc {

TPCCTransaction::TPCCTransaction(Client *client) : AsyncTransaction(client) {
}

TPCCTransaction::~TPCCTransaction() {
}

} // namespace tpcc
