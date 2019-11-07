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

TEST(BranchCompatible, CommitCompatibleUpdatedNoConflicts) {
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

  proto::Transaction *txn1 = branch.add_seq();
  *txn1 = txn;

  op = txn.add_ops();
  op->set_type(proto::OperationType::WRITE);
  op->set_key("2");
  op->set_val("val2");

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

  proto::Transaction *txn1 = branch.add_seq();
  *txn1 = txn;

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

  proto::Transaction *txn1 = branch.add_seq();
  *txn1 = txn;

  proto::Transaction *txn2 = branch.add_seq();
  *txn2 = txnn;

  std::vector<proto::Transaction> committed;
  committed.push_back(txn);

  PrintBranch(branch, std::cerr);
  PrintTransactionList(committed, std::cerr);
  std::cerr << std::endl;

  EXPECT_FALSE(CommitCompatible(branch, committed));
  EXPECT_TRUE(WaitCompatible(branch, committed));
}


TEST(BranchCompatible, Example1) {
  proto::Branch branch;
  branch.set_id(0UL);
  branch.mutable_txn()->set_id(2UL);

  proto::Transaction *txn1 = branch.add_seq();
  txn1->set_id(0UL);
  proto::Operation *op1 = txn1->add_ops();
  op1->set_type(proto::OperationType::WRITE);
  op1->set_key("44");
  op1->set_val("val1");

  proto::Transaction *txn2 = branch.add_seq(); 
  txn2->set_id(8388608UL);
  proto::Operation *op2 = txn2->add_ops();
  op2->set_type(proto::OperationType::WRITE);
  op2->set_key("723");
  op2->set_val("val2");

  proto::Transaction *txn3 = branch.add_seq(); 
  txn3->set_id(1UL);
  proto::Operation *op3 = txn3->add_ops();
  op3->set_type(proto::OperationType::WRITE);
  op3->set_key("481");
  op3->set_val("val3");

  proto::Transaction *txn4 = branch.add_seq(); 
  txn4->set_id(8388609UL);
  proto::Operation *op4 = txn4->add_ops();
  op4->set_type(proto::OperationType::WRITE);
  op4->set_key("933");
  op4->set_val("val4");

  std::vector<proto::Transaction> prepared;
  prepared.push_back(*txn1);
  prepared.push_back(*txn2);
  prepared.push_back(*txn4);
  prepared.push_back(*txn3);

  EXPECT_TRUE(CommitCompatible(branch, prepared));
}

}
