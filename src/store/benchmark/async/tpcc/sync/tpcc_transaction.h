#ifndef SYNC_TPCC_TRANSACTION_H
#define SYNC_TPCC_TRANSACTION_H

#include "store/benchmark/async/tpcc/tpcc-proto.pb.h"
#include "store/common/frontend/sync_client.h"
#include "store/common/frontend/sync_transaction.h"

namespace tpcc {


class SyncTPCCTransaction : public SyncTransaction {
 public:
  SyncTPCCTransaction();
  virtual ~SyncTPCCTransaction();

};

}  // namespace tpcc

#endif /* SYNC_TPCC_TRANSACTION_H */
