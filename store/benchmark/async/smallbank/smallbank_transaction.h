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
  SmallbankTransaction(SmallbankTransactionType type, const std::string &cust1,
      const std::string &cust2, const uint32_t timeout);

//        void CreateAccount(const std::string &name, const uint32_t customer_id);

  virtual int Execute(SyncClient &client);

  std::pair<uint32_t, bool> Bal(SyncClient &client, const std::string &name);

  bool DepositChecking(SyncClient &client, const std::string &name, const int32_t value);

  bool TransactSaving(SyncClient &client, const std::string &name, const int32_t value);

  bool Amalgamate(SyncClient &client, const std::string &name1, const std::string &name2);

  bool WriteCheck(SyncClient &client, const std::string &name, const int32_t value);

  SmallbankTransactionType GetTransactionType();
  
  bool ReadAccountRow(SyncClient &client, const std::string &name, proto::AccountRow &accountRow);

  bool ReadCheckingRow(SyncClient &client, const uint32_t customer_id, proto::CheckingRow &checkingRow);

  bool ReadSavingRow(SyncClient &client, const uint32_t customer_id, proto::SavingRow &savingRow);

  void InsertAccountRow(SyncClient &client, const std::string &name, const uint32_t customer_id);

  void InsertSavingRow(SyncClient &client, const uint32_t customer_id, const uint32_t balance);

  void InsertCheckingRow(SyncClient &client, const uint32_t customer_id, const uint32_t balance);
 
 private:
  SmallbankTransactionType type;
  std::string cust1;
  std::string cust2;
  uint32_t timeout_;

  
};

}  // namespace smallbank

#endif /* SMALLBANK_TRANSACTION_H */
