#include "store/mortystore/common.h"

#include <functional>

#include "lib/assert.h"

#include <google/protobuf/util/message_differencer.h>

bool operator==(const mortystore::proto::Branch &b1,
    const mortystore::proto::Branch &b2) {
  if (b1.id() != b2.id()) {
    return false;
  }
  if (b1.txn() != b2.txn()) {
    return false;
  }
  if (b1.deps_size() != b2.deps_size()) {
    return false;
  }
  for (const auto &kv1 : b1.deps()) {
    auto itr = b2.deps().find(kv1.first);
    if (itr == b2.deps().end() || itr->second != kv1.second) {
      Debug("Branch1 dep %lu not in branch2.", kv1.first);
      return false;
    }
  }
  return true;
}

bool operator==(const mortystore::proto::Transaction &t1,
    const mortystore::proto::Transaction &t2) {
  if (t1.id() != t2.id()) {
    return false;
  }
  if (t1.ops_size() != t2.ops_size()) {
    return false;
  }
  for (int64_t i = 0; i < t1.ops_size(); ++i) {
    if (t1.ops(i) != t2.ops(i)) {
      return false;
    }
  }
  return true;
}

bool operator!=(const mortystore::proto::Transaction &t1,
    const mortystore::proto::Transaction &t2) {
  return !(t1 == t2);
}

bool operator==(const mortystore::proto::Operation &o1,
    const mortystore::proto::Operation &o2) {
  return o1.type() == o2.type() && o1.key() == o2.key() && o1.val() == o2.val();
}

bool operator!=(const mortystore::proto::Operation &o1,
    const mortystore::proto::Operation &o2) {
  return !(o1 == o2);
}


namespace mortystore {

void hash_combine_impl(size_t& seed, size_t value) {
  seed ^= value + 0x9e3779b9 + (seed<<6) + (seed>>2);
}

void HashOperation(size_t &hash, const proto::Operation &op) {
  hash_combine_impl(hash, std::hash<int>{}(static_cast<int>(op.type())));
  hash_combine_impl(hash, std::hash<std::string>{}(op.key()));
  hash_combine_impl(hash, std::hash<std::string>{}(op.val()));
}

void HashTransaction(size_t &hash, const proto::Transaction &txn) {
  hash_combine_impl(hash, std::hash<unsigned long long>{}(txn.id()));
  for (int64_t i = 0; i < txn.ops_size(); ++i) {
    HashOperation(hash, txn.ops(i));
  }
}

void HashBranch(size_t &hash, const proto::Branch &branch) {
  hash_combine_impl(hash, std::hash<unsigned long long>{}(branch.id()));
  HashTransaction(hash, branch.txn());
  for (const auto &kv : branch.deps()) {
    size_t thash = 0UL;
    HashTransaction(thash, kv.second);
    hash = hash ^ thash;
  }
}

size_t BranchHasher::operator() (const proto::Branch &b) const {
  size_t hash = 0UL;
  HashBranch(hash, b);
  return hash;
}

bool BranchComparer::operator() (const proto::Branch &b1, const proto::Branch &b2) const {
  return b1 == b2;
}

size_t TransactionVectorHasher::operator() (const std::vector<proto::Transaction> &v) const {
  size_t hash = 0UL;
  for (const proto::Transaction &t : v) {
		HashTransaction(hash, t);
  }
  return hash;
}

bool TransactionVectorComparer::operator() (const std::vector<proto::Transaction> &v1,
			const std::vector<proto::Transaction> &v2) const {
  if (v1.size() != v2.size()) {
    return false;
  }
  for (size_t i = 0; i < v1.size(); ++i) {
    if (v1[i] != v2[i]) {
      return false;
    }
  }
  return true;
}

void PrintTransactionList(const std::vector<proto::Transaction> &txns,
    std::ostream &os) {
  for (const auto &t : txns) {
    os << t.id() << "[";
    for (const auto &o : t.ops()) {
      if (o.type() == proto::OperationType::READ) {
        os << "r";
      } else {
        os << "w";
      }
      os << "(" << o.key() << "),";
    }
    os << "],";
  }
}

void PrintBranch(const proto::Branch &branch, std::ostream &os) {
  os << branch.txn().id() << "[";
  for (const proto::Operation &o : branch.txn().ops()) {
    if (o.type() == proto::OperationType::READ) {
      os << "r(" << o.key() << "),";
    } else {
      os << "w(" << o.key() << "),";
    }
  }
  os << "]{";
  for (const auto &b : branch.deps()) {
    os << b.first << "[";
    for (const proto::Operation &o : b.second.ops()) {
      if (o.type() == proto::OperationType::READ) {
        os << "r(" << o.key() << "),";
      } else {
        os << "w(" << o.key() << "),";
      }
    }
    os << "],";
  }
  os << "}";
}

bool CommitCompatible(const proto::Branch &branch, const SpecStore &store,
    const std::vector<proto::Transaction> &seq,
    const std::set<uint64_t> &prepared_txn_ids) {
  return DepsFinalized(branch, prepared_txn_ids) &&
    ValidSubsequence(branch, store, seq);
}

bool WaitCompatible(const proto::Branch &branch, const SpecStore &store,
    const std::vector<proto::Transaction> &seq, bool ignoreLastOp) {
  return ValidSubsequence(branch, store, seq, ignoreLastOp);
}


bool DepsFinalized(const proto::Branch &branch,
    const std::set<uint64_t> &prepared_txn_ids) {
  for (const auto &dep : branch.deps()) {
    if (prepared_txn_ids.find(dep.first) == prepared_txn_ids.end()) {
      return false;
    }
  }
  return true;
}

bool ValidSubsequence(const proto::Branch &branch,
      const SpecStore &store,
      const std::vector<proto::Transaction> &seq2,
      bool ignoreLastOp) {
  // now check that all conflicting transactions are ordered the same
  const proto::Transaction *t;
  std::vector<int> ignoret;
  for (size_t i = 0; i < branch.txn().ops_size(); ++i) {
    if (ignoreLastOp && i == branch.txn().ops_size() - 1) {
      continue;
    }
    const proto::Operation &op = branch.txn().ops(i);
    if (MostRecentConflict(op, store, seq2, t)) {
      bool found = false;
      for (const auto &kv : branch.deps()) {
        const proto::Transaction &dep = kv.second;
        if (dep.id() == t->id()) {
          ignoret.clear();
          for (int64_t k = 0; k < t->ops_size(); ++k) {
            for (int64_t l = 0; l < dep.ops_size(); ++l) { 
              // TODO: this is sort of assuming that each operation in a txn is unique
              if (t->ops(k).type() == dep.ops(l).type()
                  && t->ops(k).key() == dep.ops(l).key()
                  && t->ops(k).val() == dep.ops(l).val()) {
                bool found2 = false;
                for (size_t m = 0; m < ignoret.size(); ++m) {
                  if (ignoret[m] == l) {
                    found2 = true;
                    break;
                  }
                }
                if (!found2) {
                  ignoret.push_back(l);
                  break;
                }
              }
            }
          }
          // if the dependency committed with conflicting operations that we are
          //   not aware of, we cannot commit
          if (TransactionsConflict(branch.txn(), *t, std::vector<int>(), ignoret)) {
            Debug("Found mrc %lu in deps, but has unknown ops.", t->id());
            return false;
          } else {
            found = true;
            break;
          }
        }
      }
      if (!found) {
        Debug("Did not find mrc %lu in deps.", t->id());
        return false;
      }
    }
  }
  return true;
}

bool TransactionsConflict(const proto::Transaction &txn1,
      const proto::Transaction &txn2, const std::vector<int> &ignore1,
      const std::vector<int> &ignore2) {
  size_t i1 = 0;
  for (int64_t i = 0; i < txn1.ops_size(); ++i) {
    if (i1 < ignore1.size() && i == ignore1[i1]) {
      ++i1;
      continue;
    }
    size_t j2 = 0;
    for (int64_t j = 0; j < txn2.ops_size(); ++j) {
      if (j2 < ignore2.size() && j == ignore2[j2]) {
        ++j2;
        continue;
      }
      if ((txn1.ops(i).type() == proto::OperationType::WRITE ||
          txn2.ops(j).type() == proto::OperationType::WRITE) &&
          txn1.ops(i).key() == txn2.ops(j).key()) {
        return true;
      }
    }
  }
  return false;
}

bool MostRecentConflict(const proto::Operation &op, const SpecStore &store,
    const std::vector<proto::Transaction> &seq, const proto::Transaction *&txn) {
  for (int64_t i = seq.size() - 1; i >= 0; --i) {
    for (int64_t j = seq[i].ops_size() - 1; j >= 0; --j) {
      if ((op.type() != proto::OperationType::READ
          || seq[i].ops(j).type() == proto::OperationType::WRITE) &&
          op.key() == seq[i].ops(j).key()) {
        txn = &seq[i];
        return true;
      }
    }
  }
  return store.MostRecentConflict(op, txn);
}

bool ValueOnBranch(const proto::Branch &branch, const std::string &key,
    std::string &val) {
  if (ValueInTransaction(branch.txn(), key, val)) {
    return true;
  } else if (branch.deps().size() > 0) {
    for (auto itr = branch.deps().begin(); itr != branch.deps().end(); ++itr) {
      if (ValueInTransaction(itr->second, key, val)) {
        return true;
      }
    }
  }
  return false;
}

bool ValueInTransaction(const proto::Transaction &txn, const std::string &key,
    std::string &val) {
  for (auto itr = txn.ops().rbegin(); itr != txn.ops().rend(); ++itr) {
    if (itr->type() == proto::OperationType::WRITE && itr->key() == key) {
      val = itr->val();
      return true;
    }
  }
  return false;
}

proto::Transaction _testing_txn(const std::vector<std::vector<std::string>> &txn) {
  proto::Transaction t;
  UW_ASSERT(txn.size() > 1);
  t.set_id(std::stoi(txn[0][0]));

  for (size_t i = 1; i < txn.size(); ++i) {
    UW_ASSERT(txn[i].size() == 2);
    proto::Operation *o = t.add_ops();
    *o = _testing_op(txn[i]);
  }
  return t;
}

proto::Branch _testing_branch(const std::vector<std::vector<std::vector<std::string>>> &branch) {
  proto::Branch b;
  UW_ASSERT(branch.size() > 0);
  for (size_t i = 0; i < branch.size() - 1; ++i) {
    proto::Transaction t(_testing_txn(branch[i]));
    (*b.mutable_deps())[t.id()] = t;
  }
  *b.mutable_txn() = _testing_txn(branch[branch.size() - 1]);
  return b;
}

std::vector<proto::Transaction> _testing_txns(
    const std::vector<std::vector<std::vector<std::string>>> &txns) {
  std::vector<proto::Transaction> v;
  UW_ASSERT(txns.size() > 0);
  for (size_t i = 0; i < txns.size(); ++i) {
    v.push_back(_testing_txn(txns[i]));
  }
  return v;
}

proto::Operation _testing_op(const std::vector<std::string> &op) {
  proto::Operation o;
  o.set_key(op[0]);
  o.set_val(op[1]);
  if (op[1].length() == 0) {
    o.set_type(proto::OperationType::READ);
  } else {
    o.set_type(proto::OperationType::WRITE);
  }
  return o;
}

} // namespace mortystore
