// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 *   Smallbank transactions.
 *
 **********************************************************************/

#include "store/benchmark/async/smallbank/smallbank_transaction.h"

#include "store/benchmark/async/smallbank/smallbank-proto.pb.h"
#include "store/benchmark/async/smallbank/utils.h"

namespace smallbank {

SmallbankTransaction::SmallbankTransaction(SmallbankTransactionType type)
    : SyncTransaction(10000), type(type) {}

SmallbankTransactionType SmallbankTransaction::GetTransactionType() {
  return type;
}
}  // namespace smallbank
