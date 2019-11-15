#ifndef SMALLBANK_TRANSACTION_H
#define SMALLBANK_TRANSACTION_H

#include "store/benchmark/async/smallbank/smallbank-proto.pb.h"
#include "store/common/frontend/sync_client.h"
#include "store/common/frontend/sync_transaction.h"

namespace smallbank {

enum SmallbankTransactionType {
  BALANCE,
  DEPOSIT,
  TRANSACT,
  AMALGAMATE,
  WRITE_CHECK
};

class SmallbankTransaction : public SyncTransaction {
 public:
  SmallbankTransaction(SmallbankTransactionType type);

  virtual int Execute(SyncClient &client);

  SmallbankTransactionType GetTransactionType();
 
 private:
  SmallbankTransactionType type;
  
};

}  // namespace smallbank

#endif /* SMALLBANK_TRANSACTION_H */
