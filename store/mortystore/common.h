#ifndef MORTY_COMMON_H
#define MORTY_COMMON_H

#include <iostream>
#include <vector>

#include "store/mortystore/morty-proto.pb.h"
#include "store/mortystore/specstore.h"

bool operator==(const mortystore::proto::Branch &b1,
    const mortystore::proto::Branch &b2);

bool operator==(const mortystore::proto::Transaction &t1,
    const mortystore::proto::Transaction &t2);

bool operator!=(const mortystore::proto::Transaction &t1,
    const mortystore::proto::Transaction &t2);

bool operator==(const mortystore::proto::Operation &o1,
    const mortystore::proto::Operation &o2);

bool operator!=(const mortystore::proto::Operation &o1,
    const mortystore::proto::Operation &o2);

namespace mortystore {

struct BranchHasher {
	size_t operator() (const proto::Branch &b) const;
};

struct BranchComparer {
  bool operator() (const proto::Branch &b1, const proto::Branch &b2) const;
};

struct TransactionVectorHasher {
  size_t operator() (const std::vector<proto::Transaction> &v) const;
};

struct TransactionVectorComparer {
  bool operator() (const std::vector<proto::Transaction> &v1,
			const std::vector<proto::Transaction> &v2) const;
};


void PrintBranch(const proto::Branch &branch, std::ostream &os);

void PrintTransactionList(const std::vector<proto::Transaction> &txns, std::ostream &os);

bool CommitCompatible(const proto::Branch &branch, const SpecStore &store, const std::vector<proto::Transaction> &seq, const std::set<uint64_t> &prepared_txn_ids);

bool WaitCompatible(const proto::Branch &branch, const SpecStore &store, const std::vector<proto::Transaction> &seq);

bool DepsFinalized(const proto::Branch &branch,
      const std::set<uint64_t> &prepared_txn_ids);

bool ValidSubsequence(const proto::Branch &branch, const SpecStore &store,
      const std::vector<proto::Transaction> &seq2);

bool NoConflicts(const proto::Transaction &txn,
      const std::vector<proto::Transaction> &seq);

bool TransactionsConflict(const proto::Transaction &txn1,
      const proto::Transaction &txn2,
      const std::vector<int> &ignore1 = std::vector<int>(),
      const std::vector<int> &ignore2 = std::vector<int>());

bool MostRecentConflict(const proto::Operation &op, const SpecStore &store,
    const std::vector<proto::Transaction> &seq, const proto::Transaction *&txn);

bool ValueOnBranch(const proto::Branch &branch, const std::string &key,
      std::string &val);

bool ValueInTransaction(const proto::Transaction &txn, const std::string &key,
      std::string &val);

proto::Transaction _testing_txn(const std::vector<std::vector<std::string>> &txn);
proto::Branch _testing_branch(const std::vector<std::vector<std::vector<std::string>>> &branch);
std::vector<proto::Transaction> _testing_txns(const std::vector<std::vector<std::vector<std::string>>> &txns);



} // namespace mortystore
#endif /* MORTY_COMMON_H */
