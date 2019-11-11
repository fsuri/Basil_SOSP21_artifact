#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lib/configuration.h"
#include "store/mortystore/common.h"
#include "lib/assert.h"

namespace mortystore {

proto::Transaction T(const std::vector<std::vector<std::string>> &txn) {
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

proto::Branch B(const std::vector<std::vector<std::vector<std::string>>> &branch) {
  proto::Branch b;
  UW_ASSERT(branch.size() > 0);
  for (size_t i = 0; i < branch.size() - 1; ++i) {
    proto::Transaction *t = b.add_deps();
    *t = T(branch[i]);
  }
  *b.mutable_txn() = T(branch[branch.size() - 1]);
  return b;
}

std::vector<proto::Transaction> VT(
    const std::vector<std::vector<std::vector<std::string>>> &txns) {
  std::vector<proto::Transaction> v;
  UW_ASSERT(txns.size() > 0);
  for (size_t i = 0; i < txns.size(); ++i) {
    v.push_back(T(txns[i]));
  }
  return v;
}

TEST(BranchCompatible, CommitCompatibleNoConflicts) {
  proto::Branch branch;
  branch.set_id(0UL);
  branch.mutable_txn()->set_id(1UL);
  proto::Operation *op = branch.mutable_txn()->add_ops();
  op->set_type(proto::OperationType::READ);
  op->set_key("1");
  op->set_val("");

  proto::Transaction txn;
  txn.set_id(0UL);
  op = txn.add_ops();
  op->set_type(proto::OperationType::WRITE);
  op->set_key("0");
  op->set_val("val0");

  std::vector<proto::Transaction> committed;
  committed.push_back(txn);

  std::set<uint64_t> prepared_txn_ids;
  prepared_txn_ids.insert(txn.id());

  EXPECT_TRUE(CommitCompatible(branch, committed, prepared_txn_ids));
}

TEST(BranchCompatible, CommitCompatibleConflict) {
  proto::Branch branch;
  branch.set_id(0UL);
  branch.mutable_txn()->set_id(1UL);
  proto::Operation *op = branch.mutable_txn()->add_ops();
  op->set_type(proto::OperationType::READ);
  op->set_key("0");
  op->set_val("");

  proto::Transaction txn;
  txn.set_id(0UL);
  op = txn.add_ops();
  op->set_type(proto::OperationType::WRITE);
  op->set_key("0");
  op->set_val("val0");

  std::vector<proto::Transaction> committed;
  committed.push_back(txn);

  std::set<uint64_t> prepared_txn_ids;
  prepared_txn_ids.insert(txn.id());

  EXPECT_FALSE(CommitCompatible(branch, committed, prepared_txn_ids));
}

TEST(BranchCompatible, CommitCompatibleKnownConflict) {
  proto::Branch branch;
  branch.set_id(0UL);
  branch.mutable_txn()->set_id(1UL);
  proto::Operation *op = branch.mutable_txn()->add_ops();
  op->set_type(proto::OperationType::READ);
  op->set_key("0");
  op->set_val("");

  proto::Transaction txn;
  txn.set_id(0UL);
  op = txn.add_ops();
  op->set_type(proto::OperationType::WRITE);
  op->set_key("0");
  op->set_val("val0");

  proto::Transaction *txn1 = branch.add_deps();
  *txn1 = txn;

  std::vector<proto::Transaction> committed;
  committed.push_back(txn);

  std::set<uint64_t> prepared_txn_ids;
  prepared_txn_ids.insert(txn.id());

  EXPECT_TRUE(CommitCompatible(branch, committed, prepared_txn_ids));
}


TEST(BranchCompatible, CommitCompatibleUpdatedConflict) {
  proto::Branch branch;
  branch.set_id(0UL);
  branch.mutable_txn()->set_id(1UL);
  proto::Operation *op = branch.mutable_txn()->add_ops();
  op->set_type(proto::OperationType::READ);
  op->set_key("1");
  op->set_val("");

  proto::Transaction txn;
  txn.set_id(0UL);
  op = txn.add_ops();
  op->set_type(proto::OperationType::WRITE);
  op->set_key("0");
  op->set_val("val0");

  op = txn.add_ops();
  op->set_type(proto::OperationType::WRITE);
  op->set_key("1");
  op->set_val("val1");

  std::vector<proto::Transaction> committed;
  committed.push_back(txn);

  std::set<uint64_t> prepared_txn_ids;
  prepared_txn_ids.insert(txn.id());

  EXPECT_FALSE(CommitCompatible(branch, committed, prepared_txn_ids));
}

TEST(BranchCompatible, WaitCompatible) {
  proto::Branch branch;
  branch.set_id(0UL);
  branch.mutable_txn()->set_id(2UL);
  proto::Operation *op = branch.mutable_txn()->add_ops();
  op->set_type(proto::OperationType::READ);
  op->set_key("1");
  op->set_val("");

  proto::Transaction txn;
  txn.set_id(0UL);
  op = txn.add_ops();
  op->set_type(proto::OperationType::WRITE);
  op->set_key("0");
  op->set_val("val0");

  proto::Transaction txnn;
  txnn.set_id(1UL);
  op = txnn.add_ops();
  op->set_type(proto::OperationType::WRITE);
  op->set_key("1");
  op->set_val("val1");

  proto::Transaction *txn2 = branch.add_deps();
  *txn2 = txnn;

  std::vector<proto::Transaction> committed;
  committed.push_back(txn);

  std::set<uint64_t> prepared_txn_ids;
  prepared_txn_ids.insert(txn.id());

  PrintBranch(branch, std::cerr);
  PrintTransactionList(committed, std::cerr);
  std::cerr << std::endl;

  EXPECT_FALSE(CommitCompatible(branch, committed, prepared_txn_ids));
  EXPECT_TRUE(WaitCompatible(branch, committed));
}

TEST(BranchCompatible, CommitCompatible1) {
  proto::Branch branch = B( 
    {
      {
        {"3"}, {"0", ""}
      },
      {
        {"4"}, {"28", ""}, {"28", "val4"}, {"0", ""}, {"0", "val0"}
      }
    });
  std::vector<proto::Transaction> committed = VT({
      {{"0"}, {"22", ""}, {"22", "val0"}, {"12", ""}, {"12", "val0"}, {"6", ""},
          {"6", "val0"}, {"3", "val0"}, {"15", "val0"}},
      {{"1"}, {"54", ""}, {"82", ""}, {"3", ""}, {"31", ""}, {"6", ""}, {"9", ""},
          {"66", ""}},
      {{"2"}, {"0", ""}, {"0", ""}, {"41", ""}, {"31", ""}, {"48", ""}, {"89", ""},
          {"35", ""}, {"3", ""}, {"31", ""}, {"0", ""}},
      {{"3"}, {"0", ""}}
    });

  std::set<uint64_t> prepared_txn_ids;
  for (const auto &txn : committed) {
    prepared_txn_ids.insert(txn.id());
  }

  PrintBranch(branch, std::cerr);
  std::cerr << std::endl;
  PrintTransactionList(committed, std::cerr);
  std::cerr << std::endl;

  EXPECT_TRUE(CommitCompatible(branch, committed, prepared_txn_ids));
}


}
