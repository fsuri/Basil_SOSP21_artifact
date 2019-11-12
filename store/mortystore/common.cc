#include "store/mortystore/common.h"

#include "lib/assert.h"

#include <google/protobuf/util/message_differencer.h>

bool operator==(const mortystore::proto::Branch &b1,
    const mortystore::proto::Branch &b2) {
  return b1.txn().id() == b2.txn().id()
    && google::protobuf::util::MessageDifferencer::Equals(b1, b2);
}

bool operator==(const mortystore::proto::Transaction &t1,
    const mortystore::proto::Transaction &t2) {
  return t1.id() == t2.id()
    && google::protobuf::util::MessageDifferencer::Equals(t1, t2);
}

namespace mortystore {

size_t BranchHasher::operator() (const proto::Branch &b) const {
  return b.txn().id();
}

bool BranchComparer::operator() (const proto::Branch &b1, const proto::Branch &b2) const {
  return b1 == b2;
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
  for (const proto::Transaction &b : branch.deps()) {
    os << b.id() << "[";
    for (const proto::Operation &o : b.ops()) {
      if (o.type() == proto::OperationType::READ) {
        os << "r(" << o.key() << "),";
      } else {
        os << "w(" << o.key() << "),";
      }
    }
    os << "],";
  }
  os << branch.txn().id() << "[";
  for (const proto::Operation &o : branch.txn().ops()) {
    if (o.type() == proto::OperationType::READ) {
      os << "r(" << o.key() << "),";
    } else {
      os << "w(" << o.key() << "),";
    }
  }
  os << "]";
}

bool CommitCompatible(const proto::Branch &branch,
    const std::vector<proto::Transaction> &seq,
    const std::set<uint64_t> &prepared_txn_ids) {
  std::vector<proto::Transaction> seq3;
  for (const proto::Transaction &b : branch.deps()) {
    seq3.push_back(b);
  }
  return DepsFinalized(branch, prepared_txn_ids) &&
    ValidSubsequence(branch.txn(), seq3, seq);
}

bool WaitCompatible(const proto::Branch &branch,
    const std::vector<proto::Transaction> &seq) {
  std::vector<proto::Transaction> seq3;
  for (const proto::Transaction &b : branch.deps()) {
    seq3.push_back(b);
  }
  return ValidSubsequence(branch.txn(), seq3, seq);
}


bool DepsFinalized(const proto::Branch &branch,
    const std::set<uint64_t> &prepared_txn_ids) {
  for (const auto &dep : branch.deps()) {
    if (prepared_txn_ids.find(dep.id()) == prepared_txn_ids.end()) {
      return false;
    }
  }
  return true;
}

bool ValidSubsequence(const proto::Transaction &txn,
      const std::vector<proto::Transaction> &seq1,
      const std::vector<proto::Transaction> &seq2) {
  // now check that all conflicting transactions are ordered the same
  for (const proto::Operation &op : txn.ops()) {
    proto::Transaction t;
    if (MostRecentConflict(op, seq2, t)) {
      bool found = false;
      for (const proto::Transaction &dep : seq1) {
        if (dep.id() == t.id()) {
          proto::Transaction txn1(t);
          google::protobuf::RepeatedPtrField<proto::Operation> ops(txn1.ops());
          for (int l = 0; l < dep.ops().size(); ++l) {
            const proto::Operation &op2 = dep.ops()[l];
            auto itr = std::find_if(ops.begin(), ops.end(),
                [op2](const proto::Operation &op) {
                    return op.type() == op2.type() && op.key() == op2.key() &&
                        op.val() == op2.val();
                  });
            if (itr != ops.end()) {
              ops.erase(itr);
            }
          }
          *txn1.mutable_ops() = ops;
          // if the dependency committed with conflicting operations that we are
          //   not aware of, we cannot commit
          if (TransactionsConflict(txn, txn1)) {
            return false;
          } else {
            found = true;
            break;
          }
        }
      }
      if (!found) {
        return false;
      }
    }
  }
  return true;
}

bool TransactionsConflict(const proto::Transaction &txn1,
      const proto::Transaction &txn2) {
  std::set<std::string> rs1;
  std::set<std::string> ws1;
  for (const proto::Operation &op : txn1.ops()) {
    if (op.type() == proto::OperationType::READ) {
      rs1.insert(op.key());
    } else {
      ws1.insert(op.key());
    }
  }
  std::set<std::string> rs2;
  std::set<std::string> ws2;
  for (const proto::Operation &op : txn2.ops()) {
    if (op.type() == proto::OperationType::READ) {
      rs2.insert(op.key());
    } else {
      ws2.insert(op.key());
    }
  }
  std::vector<std::string> rs1ws2;
  std::vector<std::string> rs2ws1;
  std::vector<std::string> ws1ws2;
  std::set_intersection(rs1.begin(), rs1.end(), ws2.begin(), ws2.end(),
      std::back_inserter(rs1ws2));
  if (rs1ws2.size() > 0) {
    return true;
  }
  std::set_intersection(rs2.begin(), rs2.end(), ws1.begin(), ws1.end(),
      std::back_inserter(rs2ws1));
  if (rs2ws1.size() > 0) {
    return true;
  }
  std::set_intersection(ws1.begin(), ws1.end(), ws2.begin(), ws2.end(),
      std::back_inserter(ws1ws2));
  if (ws1ws2.size() > 0) {
    return true;
  } else {
    return false;
  }
}

bool MostRecentConflict(const proto::Operation &op,
    const std::vector<proto::Transaction> &seq, proto::Transaction &txn) {
  for (auto itr = seq.rbegin(); itr != seq.rend(); ++itr) {
    for (auto jtr = itr->ops().rbegin(); jtr != itr->ops().rend(); ++jtr) {
      if ((op.type() != proto::OperationType::READ
          || jtr->type() == proto::OperationType::WRITE) &&
          op.key() == jtr->key()) {
        txn = *itr;
        return true;
      }
    }
  }
  return false;
}

void ValueOnBranch(const proto::Branch &branch, const std::string &key,
    std::string &val) {
  if (ValueInTransaction(branch.txn(), key, val)) {
    return;
  } else if (branch.deps().size() > 0) {
    for (auto itr = branch.deps().rbegin(); itr != branch.deps().rend(); ++itr) {
      if (ValueInTransaction(*itr, key, val)) {
        return;
      }
    }
  }
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
    o->set_key(txn[i][0]);
    o->set_val(txn[i][1]);
    if (txn[i][1].length() == 0) {
      o->set_type(proto::OperationType::READ);
    } else {
      o->set_type(proto::OperationType::WRITE);
    }
  }
  return t;
}

proto::Branch _testing_branch(const std::vector<std::vector<std::vector<std::string>>> &branch) {
  proto::Branch b;
  UW_ASSERT(branch.size() > 0);
  for (size_t i = 0; i < branch.size() - 1; ++i) {
    proto::Transaction *t = b.add_deps();
    *t = _testing_txn(branch[i]);
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

} // namespace mortystore
