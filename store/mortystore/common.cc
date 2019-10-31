#include "store/mortystore/common.h"

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

void PrintBranch(const proto::Branch &branch) {
  for (const proto::Transaction &b : branch.seq()) {
    std::cerr << b.id() << "[";
    for (const proto::Operation &o : b.ops()) {
      if (o.type() == proto::OperationType::READ) {
        std::cerr << "r(" << o.key() << "),";
      } else {
        std::cerr << "w(" << o.key() << "),";
      }
    }
    std::cerr << "],";
  }
  std::cerr << branch.txn().id() << "[";
  for (const proto::Operation &o : branch.txn().ops()) {
    if (o.type() == proto::OperationType::READ) {
      std::cerr << "r(" << o.key() << "),";
    } else {
      std::cerr << "w(" << o.key() << "),";
    }
  }
  std::cerr << "]" << std::endl;
}

bool CommitCompatible(const proto::Branch &branch,
    const std::vector<proto::Transaction> &seq) {
  std::vector<proto::Transaction> seq3;
  std::vector<proto::Transaction> seq4(seq);
  for (const proto::Transaction &b : branch.seq()) {
    seq3.push_back(b);
    auto itr = std::find_if(seq4.begin(), seq4.end(), [&](const proto::Transaction &other) {
          return google::protobuf::util::MessageDifferencer::Equals(b, other);
        });

    if (itr != seq4.end()) {
      seq4.erase(itr);
    }
  }
  return ValidSubsequence(branch.txn(), seq3, seq) &&
      NoConflicts(branch.txn(), seq4);
}

bool WaitCompatible(const proto::Branch &branch, const std::vector<proto::Transaction> &seq) {
  std::vector<proto::Transaction> seq3;
  std::vector<proto::Transaction> seq4(seq);
  for (const proto::Transaction &b : branch.seq()) {
    auto itr = std::find_if(seq4.begin(), seq4.end(), [&](const proto::Transaction &other) {
          return google::protobuf::util::MessageDifferencer::Equals(b, other);
        });
    if (itr != seq4.end()) {
      seq4.erase(itr);
    }
    auto itr2 = std::find_if(seq.begin(), seq.end(), [&](const proto::Transaction &other) {
          return google::protobuf::util::MessageDifferencer::Equals(b, other);
        });
    if (itr2 != seq.end()) {
      seq3.push_back(b);
    }
  }
  return ValidSubsequence(branch.txn(), seq3, seq) &&
      NoConflicts(branch.txn(), seq4);
}


bool ValidSubsequence(const proto::Transaction &txn,
      const std::vector<proto::Transaction> &seq1,
      const std::vector<proto::Transaction> &seq2) {
  int64_t k = -1;
  for (size_t i = 0; i < seq1.size(); ++i) {
    bool found = false;
    for (size_t j =  k + 1; j < seq2.size(); ++j) {
      if (seq1[i].id() == seq2[j].id()) {
        proto::Transaction txn1(seq2[j]);
        google::protobuf::RepeatedPtrField<proto::Operation> ops(txn1.ops());
        for (int l = 0; l < seq1[i].ops().size(); ++l) {
          const proto::Operation &op2 = seq1[i].ops()[l];
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
        if (TransactionsConflict(txn, txn1)) {
          return false;
        } else {
          k = j;
          found = true;
        }
      }
    }
    if (!found) {
      return false;
    }
  }
  return true;
}

bool NoConflicts(const proto::Transaction &txn,
      const std::vector<proto::Transaction> &seq) {
  for (size_t i = 0; i < seq.size(); ++i) {
    if (TransactionsConflict(txn, seq[i])) {
      return false;
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

void ValueOnBranch(const proto::Branch &branch, const std::string &key,
    std::string &val) {
  if (ValueInTransaction(branch.txn(), key, val)) {
    return;
  } else if (branch.seq().size() > 0) {
    for (auto itr = branch.seq().rbegin(); itr != branch.seq().rend(); ++itr) {
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


} // namespace mortystore
