#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lib/configuration.h"
#include "store/mortystore/server.h"

namespace mortystore {

TEST(BranchGenerator, NoCommittedNoConcurrentNewBranch) {
  std::vector<proto::Transaction> committed;

  BranchGenerator generator;

  proto::Branch branch;
  branch.set_id(0UL);
  branch.mutable_txn()->set_id(0UL);
  proto::Operation *op = branch.mutable_txn()->add_ops();
  proto::OperationType type = proto::OperationType::READ;
  std::string key = "0";
  op->set_type(type);
  op->set_key(key);;
  op->set_val("");

  std::vector<proto::Branch> new_branches;
  generator.GenerateBranches(branch, type, key, committed, new_branches);

  EXPECT_EQ(new_branches.size(), 1UL);
  EXPECT_TRUE(new_branches[0] == branch);
}

TEST(BranchGenerator, NoCommittedNoConcurrentUpdatedBranch) {
  std::vector<proto::Transaction> committed;

  BranchGenerator generator;

  proto::Branch branch;
  branch.set_id(0UL);
  branch.mutable_txn()->set_id(0UL);
  proto::Operation *op = branch.mutable_txn()->add_ops();
  proto::OperationType type = proto::OperationType::READ;
  std::string key = "0";
  op->set_type(type);
  op->set_key(key);;
  op->set_val("");

  generator.AddPendingRead(key, branch);

  // update branch with another operation
  proto::Operation *op2 = branch.mutable_txn()->add_ops();
  op2->set_type(type);
  op2->set_key(key);;
  op2->set_val("");

  std::vector<proto::Branch> new_branches;
  generator.GenerateBranches(branch, type, key, committed, new_branches);

  EXPECT_EQ(new_branches.size(), 1UL);
  EXPECT_TRUE(new_branches[0] == branch);
}

TEST(BranchGenerator, OneCommittedNoConcurrentNewBranch) {
  std::vector<proto::Transaction> committed;

  BranchGenerator generator;

  proto::Transaction txn1;
  txn1.set_id(0UL);
  proto::Operation *op1 = txn1.add_ops();
  proto::OperationType type1 = proto::OperationType::READ;
  std::string key1 = "0";
  op1->set_type(type1);
  op1->set_key(key1);;
  op1->set_val("");
  committed.push_back(txn1);

  proto::Branch branch;
  branch.set_id(0UL);
  branch.mutable_txn()->set_id(1UL);
  proto::Operation *op2 = branch.mutable_txn()->add_ops();
  op2->set_type(type1);
  op2->set_key(key1);;
  op2->set_val("");

  std::vector<proto::Branch> new_branches;
  generator.GenerateBranches(branch, op2->type(), op2->key(), committed, new_branches);

  EXPECT_EQ(new_branches.size(), 1UL);

  proto::Branch generated;
  generated.set_id(0UL);
  proto::Transaction *txn2 = generated.add_seq();
  *txn2 = txn1;
  *generated.mutable_txn() = branch.txn();
  EXPECT_TRUE(new_branches[0] == generated);
}

TEST(BranchGenerator, NoCommittedOneConcurrentNewBranch) {
  std::vector<proto::Transaction> committed;

  BranchGenerator generator;

  proto::Branch branch1;
  branch1.set_id(0UL);
  branch1.mutable_txn()->set_id(0UL);
  proto::OperationType type1 = proto::OperationType::WRITE;
  std::string key1 = "0";
  std::string val1 = "val1";
  proto::Operation *op1 = branch1.mutable_txn()->add_ops();
  op1->set_type(type1);
  op1->set_key(key1);;
  op1->set_val(val1);

  std::vector<proto::Branch> new_branches;
  generator.AddPendingWrite(key1, branch1);
  generator.GenerateBranches(branch1, type1, key1, committed, new_branches);
  new_branches.clear();

  proto::Branch branch2;
  branch2.set_id(0UL);
  branch2.mutable_txn()->set_id(1UL);
  proto::OperationType type2 = proto::OperationType::WRITE;
  std::string key2 = "0";
  std::string val2 = "val2";
  proto::Operation *op2 = branch2.mutable_txn()->add_ops();
  op2->set_type(type2);
  op2->set_key(key2);
  op2->set_val(val2);

  generator.AddPendingWrite(key2, branch2);
  generator.GenerateBranches(branch2, type2, key2, committed, new_branches);

  EXPECT_EQ(new_branches.size(), 3UL);
  proto::Branch generated2(branch2), generated3(branch1), generated4(branch2);
  proto::Transaction *txn3 = generated3.add_seq();
  *txn3 = branch2.txn();
  proto::Transaction *txn4 = generated4.add_seq();
  *txn4 = branch1.txn();
  EXPECT_TRUE(std::find_if(new_branches.begin(), new_branches.end(),
      [&](const proto::Branch &b){
        return b == generated2;
      }) != new_branches.end());
  EXPECT_TRUE(std::find_if(new_branches.begin(), new_branches.end(),
      [&](const proto::Branch &b){
        return b == generated3;
      }) != new_branches.end());
  EXPECT_TRUE(std::find_if(new_branches.begin(), new_branches.end(),
      [&](const proto::Branch &b){
        return b == generated4;
      }) != new_branches.end());
}

TEST(BranchGenerator, OneCommittedOneConcurrentNewBranch) {
  std::vector<proto::Transaction> committed;

  proto::Transaction txn0;
  txn0.set_id(0UL);
  proto::Operation *op0 = txn0.add_ops();
  proto::OperationType type0 = proto::OperationType::WRITE;
  std::string key0 = "0";
  op0->set_type(type0);
  op0->set_key(key0);;
  op0->set_val("val0");
  committed.push_back(txn0);

  BranchGenerator generator;

  proto::Branch branch1;
  branch1.set_id(0UL);
  branch1.mutable_txn()->set_id(1UL);
  proto::OperationType type1 = proto::OperationType::WRITE;
  std::string key1 = "0";
  std::string val1 = "val1";
  proto::Operation *op1 = branch1.mutable_txn()->add_ops();
  op1->set_type(type1);
  op1->set_key(key1);;
  op1->set_val(val1);

  std::vector<proto::Branch> new_branches;
  generator.AddPendingWrite(key1, branch1);
  generator.GenerateBranches(branch1, type1, key1, committed, new_branches);
  new_branches.clear();

  proto::Branch branch2;
  branch2.set_id(0UL);
  branch2.mutable_txn()->set_id(2UL);
  proto::OperationType type2 = proto::OperationType::WRITE;
  std::string key2 = "0";
  std::string val2 = "val2";
  proto::Operation *op2 = branch2.mutable_txn()->add_ops();
  op2->set_type(type2);
  op2->set_key(key2);
  op2->set_val(val2);

  generator.AddPendingWrite(key2, branch2);
  generator.GenerateBranches(branch2, type2, key2, committed, new_branches);

  EXPECT_EQ(new_branches.size(), 3UL);
  proto::Branch generated2, generated3, generated4;
  generated2.set_id(0UL);
  generated3.set_id(0UL);
  generated4.set_id(0UL);
  proto::Transaction *txn = generated2.add_seq();
  *txn = txn0;
  txn = generated3.add_seq();
  *txn = txn0;
  txn = generated4.add_seq();
  *txn = txn0;

  txn = generated3.add_seq();
  *txn = branch2.txn();
  txn = generated4.add_seq();
  *txn = branch1.txn();

  *generated2.mutable_txn() = branch2.txn();
  *generated3.mutable_txn() = branch1.txn();
  *generated4.mutable_txn() = branch2.txn();

  PrintBranch(generated2, std::cerr);
  PrintBranch(generated3, std::cerr);
  PrintBranch(generated4, std::cerr);
  EXPECT_TRUE(std::find_if(new_branches.begin(), new_branches.end(),
      [&](const proto::Branch &b){
        return b == generated2;
      }) != new_branches.end());
  EXPECT_TRUE(std::find_if(new_branches.begin(), new_branches.end(),
      [&](const proto::Branch &b){
        return b == generated3;
      }) != new_branches.end());
  EXPECT_TRUE(std::find_if(new_branches.begin(), new_branches.end(),
      [&](const proto::Branch &b){
        return b == generated4;
      }) != new_branches.end());
}

TEST(BranchGenerator, OneCommittedOneConcurrentUpdatedBranch) {
  std::vector<proto::Transaction> committed;

  proto::Transaction txn0;
  txn0.set_id(0UL);
  proto::Operation *op0 = txn0.add_ops();
  proto::OperationType type0 = proto::OperationType::WRITE;
  std::string key0 = "0";
  op0->set_type(type0);
  op0->set_key(key0);;
  op0->set_val("val0");
  committed.push_back(txn0);

  BranchGenerator generator;

  proto::Branch branch1;
  branch1.set_id(0UL);
  branch1.mutable_txn()->set_id(1UL);
  proto::OperationType type1 = proto::OperationType::WRITE;
  std::string key1 = "0";
  std::string val1 = "val1";
  proto::Operation *op1 = branch1.mutable_txn()->add_ops();
  op1->set_type(type1);
  op1->set_key(key1);;
  op1->set_val(val1);

  std::vector<proto::Branch> new_branches;
  generator.AddPendingWrite(key1, branch1);
  generator.GenerateBranches(branch1, type1, key1, committed, new_branches);
  new_branches.clear();

  proto::Branch branch2;
  branch2.set_id(0UL);
  branch2.mutable_txn()->set_id(2UL);
  proto::OperationType type2 = proto::OperationType::WRITE;
  std::string key2 = "0";
  std::string val2 = "val2";
  proto::Operation *op2 = branch2.mutable_txn()->add_ops();
  op2->set_type(type2);
  op2->set_key(key2);
  op2->set_val(val2);

  generator.AddPendingWrite(key2, branch2);
  generator.GenerateBranches(branch2, type2, key2, committed, new_branches);
  new_branches.clear();

  proto::Operation *op3 = branch1.mutable_txn()->add_ops();
  proto::OperationType type3 = proto::OperationType::WRITE;
  std::string key3 = "0";
  std::string val3 = "val3";
  op3->set_type(type3);
  op3->set_key(key3);
  op3->set_val(val3);
  proto::Transaction *ctxn = branch1.add_seq();
  *ctxn = txn0;

  std::cerr << "#### ACTUAL GENERATION ####" << std::endl;
  generator.AddPendingWrite(key3, branch1);
  generator.GenerateBranches(branch1, type3, key3, committed, new_branches);

  EXPECT_EQ(new_branches.size(), 2UL);
  proto::Branch generated2, generated3;
  generated2.set_id(0UL);
  generated3.set_id(0UL);
  proto::Transaction *txn = generated2.add_seq();
  *txn = txn0;
  txn = generated3.add_seq();
  *txn = txn0;

  txn = generated3.add_seq();
  *txn = branch1.txn();

  *generated2.mutable_txn() = branch1.txn();
  *generated3.mutable_txn() = branch2.txn();

  PrintBranch(generated2, std::cerr);
  PrintBranch(generated3, std::cerr);
  EXPECT_TRUE(std::find_if(new_branches.begin(), new_branches.end(),
      [&](const proto::Branch &b){
        return b == generated2;
      }) != new_branches.end());
  EXPECT_TRUE(std::find_if(new_branches.begin(), new_branches.end(),
      [&](const proto::Branch &b){
        return b == generated3;
      }) != new_branches.end());
}



}
