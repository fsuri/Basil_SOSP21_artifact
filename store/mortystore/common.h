#ifndef MORTY_COMMON_H
#define MORTY_COMMON_H

#include "store/mortystore/morty-proto.pb.h"

bool operator==(const mortystore::proto::Branch &b1,
    const mortystore::proto::Branch &b2);

bool operator==(const mortystore::proto::Transaction &t1,
    const mortystore::proto::Transaction &t2);

namespace mortystore {

struct BranchHasher {
	size_t operator() (const proto::Branch &b) const;
};

struct BranchComparer {
  bool operator() (const proto::Branch &b1, const proto::Branch &b2) const;
};

void PrintBranch(const proto::Branch &branch);

bool CommitCompatible(const proto::Branch &branch, const std::vector<proto::Transaction> &seq);

bool WaitCompatible(const proto::Branch &branch, const std::vector<proto::Transaction> &seq);

bool ValidSubsequence(const proto::Transaction &txn,
      const std::vector<proto::Transaction> &seq1,
      const std::vector<proto::Transaction> &seq2);

bool NoConflicts(const proto::Transaction &txn,
      const std::vector<proto::Transaction> &seq);

bool TransactionsConflict(const proto::Transaction &txn1,
      const proto::Transaction &txn2);

void ValueOnBranch(const proto::Branch &branch, const std::string &key,
      std::string &val);

bool ValueInTransaction(const proto::Transaction &txn, const std::string &key,
      std::string &val);

} // namespace mortystore
#endif /* MORTY_COMMON_H */
