#include "store/benchmark/async/tpcc/sync/tpcc_transaction.h"

namespace tpcc {

SyncTPCCTransaction::SyncTPCCTransaction(uint32_t timeout) : SyncTransaction(timeout) {
}

SyncTPCCTransaction::~SyncTPCCTransaction() {
}

} // namespace tpcc
