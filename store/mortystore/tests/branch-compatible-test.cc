#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lib/configuration.h"
#include "store/mortystore/common.h"

namespace mortystore {

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

  EXPECT_TRUE(CommitCompatible(branch, committed));
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

  EXPECT_FALSE(CommitCompatible(branch, committed));
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

  EXPECT_TRUE(CommitCompatible(branch, committed));
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

  EXPECT_FALSE(CommitCompatible(branch, committed));
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

  PrintBranch(branch, std::cerr);
  PrintTransactionList(committed, std::cerr);
  std::cerr << std::endl;

  EXPECT_FALSE(CommitCompatible(branch, committed));
  EXPECT_TRUE(WaitCompatible(branch, committed));
}

}
