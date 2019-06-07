#include "store/benchmark/async/tpcc/tpcc_transaction.h"

#include "store/benchmark/async/tpcc/tpcc-proto.pb.h"

namespace tpcc {

TPCCTransaction::TPCCTransaction() : AsyncTransaction(0) {
}

TPCCTransaction::~TPCCTransaction() {
}

} // namespace tpcc
