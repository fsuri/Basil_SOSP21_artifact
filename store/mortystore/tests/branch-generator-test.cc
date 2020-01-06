#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lib/configuration.h"
#include "store/mortystore/server.h"

namespace mortystore {

proto::Transaction T(const std::vector<std::vector<std::string>> &txn) {
  proto::Transaction t;
  for (const auto &op : txn) {
    UW_ASSERT(op.size() == 2);
    proto::Operation *o = t.add_ops();
    o->set_key(op[0]);
    o->set_val(op[1]);
    if (op[1].length() == 0) {
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
    proto::Transaction t = T(branch[i]);
    (*b.mutable_deps())[t.id()] = t;
  }
  *b.mutable_txn() = T(branch[branch.size() - 1]);
  return b;
}

TEST(BranchGenerator, NoCommittedNoConcurrentNewBranch) {
  SpecStore ss;

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
  
  generator.AddPending(branch);
  generator.GenerateBranches(branch, type, key, ss, new_branches);

  EXPECT_EQ(new_branches.size(), 1UL);
  EXPECT_TRUE(new_branches[0] == branch);
}

TEST(BranchGenerator, NoCommittedNoConcurrentUpdatedBranch) {
  SpecStore ss;

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

  generator.AddPending(branch);
  std::vector<proto::Branch> new_branches;
  generator.GenerateBranches(branch, type, key, ss, new_branches);

  // update branch with another operation
  proto::Operation *op2 = branch.mutable_txn()->add_ops();
  op2->set_type(type);
  op2->set_key(key);;
  op2->set_val("");

  new_branches.clear();
  generator.AddPending(branch);
  generator.GenerateBranches(branch, type, key, ss, new_branches);

  EXPECT_EQ(new_branches.size(), 1UL);
  EXPECT_TRUE(new_branches[0] == branch);
}

TEST(BranchGenerator, OneCommittedNoConcurrentNewBranch) {
  SpecStore ss;

  BranchGenerator generator;

  proto::Transaction txn1;
  txn1.set_id(0UL);
  proto::Operation *op1 = txn1.add_ops();
  proto::OperationType type1 = proto::OperationType::READ;
  std::string key1 = "0";
  op1->set_type(type1);
  op1->set_key(key1);;
  op1->set_val("");
  ss.ApplyTransaction(txn1);

  proto::Branch branch;
  branch.set_id(0UL);
  branch.mutable_txn()->set_id(1UL);
  proto::Operation *op2 = branch.mutable_txn()->add_ops();
  op2->set_type(type1);
  op2->set_key(key1);;
  op2->set_val("");

  std::vector<proto::Branch> new_branches;
  generator.AddPending(branch);
  generator.GenerateBranches(branch, op2->type(), op2->key(), ss, new_branches);

  EXPECT_EQ(new_branches.size(), 1UL);

  proto::Branch generated;
  generated.set_id(0UL);
  *generated.mutable_txn() = branch.txn();
  EXPECT_TRUE(new_branches[0] == generated);
}

TEST(BranchGenerator, OneCommittedConflictWW) {
  SpecStore ss;

  BranchGenerator generator;

  proto::Transaction txn1;
  txn1.set_id(0UL);
  proto::Operation *op1 = txn1.add_ops();
  proto::OperationType type1 = proto::OperationType::WRITE;
  std::string key1 = "0";
  op1->set_type(type1);
  op1->set_key(key1);;
  op1->set_val("");
  ss.ApplyTransaction(txn1);

  proto::Branch branch;
  branch.set_id(0UL);
  branch.mutable_txn()->set_id(1UL);
  proto::Operation *op2 = branch.mutable_txn()->add_ops();
  op2->set_type(type1);
  op2->set_key(key1);;
  op2->set_val("");

  std::vector<proto::Branch> new_branches;
  generator.AddPending(branch);
  generator.GenerateBranches(branch, op2->type(), op2->key(), ss, new_branches);

  EXPECT_EQ(new_branches.size(), 1UL);

  proto::Branch generated;
  generated.set_id(0UL);
  *generated.mutable_txn() = branch.txn();
  (*generated.mutable_deps())[txn1.id()] = txn1;
  EXPECT_TRUE(new_branches[0] == generated);
}

TEST(BranchGenerator, OneCommittedConflictRW) {
  SpecStore ss;

  BranchGenerator generator;

  proto::Transaction txn1;
  txn1.set_id(0UL);
  proto::Operation *op1 = txn1.add_ops();
  op1->set_type(proto::OperationType::WRITE);
  op1->set_key("0");;
  op1->set_val("val0");
  ss.ApplyTransaction(txn1);

  proto::Branch branch;
  branch.set_id(0UL);
  branch.mutable_txn()->set_id(1UL);
  proto::Operation *op2 = branch.mutable_txn()->add_ops();
  op2->set_type(proto::OperationType::READ);
  op2->set_key("0");;
  op2->set_val("");

  std::vector<proto::Branch> new_branches;
  generator.AddPending(branch);
  generator.GenerateBranches(branch, op2->type(), op2->key(), ss, new_branches);

  EXPECT_EQ(new_branches.size(), 1UL);

  proto::Branch generated;
  generated.set_id(0UL);
  *generated.mutable_txn() = branch.txn();
  (*generated.mutable_deps())[txn1.id()] = txn1;
  EXPECT_TRUE(new_branches[0] == generated);
}

TEST(BranchGenerator, OneCommittedConflictRWUpdated) {
  SpecStore ss;

  BranchGenerator generator;

  proto::Transaction txn1;
  txn1.set_id(0UL);
  proto::Operation *op1 = txn1.add_ops();
  op1->set_type(proto::OperationType::WRITE);
  op1->set_key("0");;
  op1->set_val("val0");
  ss.ApplyTransaction(txn1);

  proto::Branch branch;
  branch.set_id(0UL);
  branch.mutable_txn()->set_id(1UL);
  proto::Operation *op2 = branch.mutable_txn()->add_ops();
  op2->set_type(proto::OperationType::READ);
  op2->set_key("1");;
  op2->set_val("");

  std::vector<proto::Branch> new_branches;
  generator.AddPending(branch);
  generator.GenerateBranches(branch, op2->type(), op2->key(), ss, new_branches);

  EXPECT_EQ(new_branches.size(), 1UL);

  proto::Branch generated;
  generated.set_id(0UL);
  *generated.mutable_txn() = branch.txn();
  EXPECT_TRUE(new_branches[0] == generated);

  proto::Operation *op3 = branch.mutable_txn()->add_ops();
  op3->set_type(proto::OperationType::READ);
  op3->set_key("0");
  op3->set_val("");

  new_branches.clear();
  generator.AddPending(branch);
  generator.GenerateBranches(branch, op3->type(), op3->key(), ss, new_branches);

  EXPECT_EQ(new_branches.size(), 1UL);

  *generated.mutable_txn() = branch.txn();
  (*generated.mutable_deps())[txn1.id()] = txn1;
  
  EXPECT_TRUE(new_branches[0] == generated);
}




TEST(BranchGenerator, NoCommittedOneConcurrentNewBranch) {
  SpecStore ss;

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
  generator.AddPending(branch1);
  generator.GenerateBranches(branch1, type1, key1, ss, new_branches);
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

  generator.AddPending(branch2);
  generator.GenerateBranches(branch2, type2, key2, ss, new_branches);

  EXPECT_EQ(new_branches.size(), 3UL);
  proto::Branch generated2(branch2), generated3(branch1), generated4(branch2);
  (*generated3.mutable_deps())[branch2.txn().id()] = branch2.txn();
  (*generated4.mutable_deps())[branch1.txn().id()] = branch1.txn();
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
  SpecStore ss;
  proto::Transaction txn0;
  txn0.set_id(0UL);
  proto::Operation *op0 = txn0.add_ops();
  proto::OperationType type0 = proto::OperationType::WRITE;
  std::string key0 = "0";
  op0->set_type(type0);
  op0->set_key(key0);;
  op0->set_val("val0");
  ss.ApplyTransaction(txn0);

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
  generator.AddPending(branch1);
  generator.GenerateBranches(branch1, type1, key1, ss, new_branches);
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

  generator.AddPending(branch2);
  generator.GenerateBranches(branch2, type2, key2, ss, new_branches);

  EXPECT_EQ(new_branches.size(), 3UL);
  proto::Branch generated2, generated3, generated4;
  // [0, 2], [0, 1, 2], [0, 2, 1]
  generated2.set_id(0UL);
  generated3.set_id(0UL);
  generated4.set_id(0UL);
  (*generated2.mutable_deps())[txn0.id()] = txn0;
  (*generated3.mutable_deps())[branch1.txn().id()] = branch1.txn();
  (*generated4.mutable_deps())[branch2.txn().id()] = branch2.txn();

  *generated2.mutable_txn() = branch2.txn();
  *generated3.mutable_txn() = branch2.txn();
  *generated4.mutable_txn() = branch1.txn();

  PrintBranch(generated2, std::cerr);
  std::cerr << std::endl;
  PrintBranch(generated3, std::cerr);
  std::cerr << std::endl;
  PrintBranch(generated4, std::cerr);
  std::cerr << std::endl;
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
  SpecStore ss;

  proto::Transaction txn0;
  txn0.set_id(0UL);
  proto::Operation *op0 = txn0.add_ops();
  proto::OperationType type0 = proto::OperationType::WRITE;
  std::string key0 = "0";
  op0->set_type(type0);
  op0->set_key(key0);;
  op0->set_val("val0");
  ss.ApplyTransaction(txn0);

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
  generator.AddPending(branch1);
  generator.GenerateBranches(branch1, type1, key1, ss, new_branches);
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

  generator.AddPending(branch2);
  generator.GenerateBranches(branch2, type2, key2, ss, new_branches);
  new_branches.clear();

  proto::Operation *op3 = branch1.mutable_txn()->add_ops();
  proto::OperationType type3 = proto::OperationType::WRITE;
  std::string key3 = "0";
  std::string val3 = "val3";
  op3->set_type(type3);
  op3->set_key(key3);
  op3->set_val(val3);
  (*branch1.mutable_deps())[txn0.id()] = txn0;

  generator.AddPending(branch1);
  std::cerr << "###################" << std::endl;
  generator.GenerateBranches(branch1, type3, key3, ss, new_branches);
  std::cerr << "###################" << std::endl;

  EXPECT_EQ(new_branches.size(), 2UL);
  PrintBranch(new_branches[0], std::cerr);
  std::cerr << std::endl;
  PrintBranch(new_branches[1], std::cerr);
  std::cerr << std::endl;

  proto::Branch generated2, generated3;
  // [0, 1], [0, 1, 2]
  generated2.set_id(0UL);
  generated3.set_id(0UL);
  (*generated2.mutable_deps())[txn0.id()] = txn0;
  (*generated3.mutable_deps())[branch1.txn().id()] = branch1.txn();

  *generated2.mutable_txn() = branch1.txn();
  *generated3.mutable_txn() = branch2.txn();

  PrintBranch(generated2, std::cerr);
  std::cerr << std::endl;
  PrintBranch(generated3, std::cerr);
  std::cerr << std::endl;
  EXPECT_TRUE(std::find_if(new_branches.begin(), new_branches.end(),
      [&](const proto::Branch &b){
        return b == generated2;
      }) != new_branches.end());
  EXPECT_TRUE(std::find_if(new_branches.begin(), new_branches.end(),
      [&](const proto::Branch &b){
        return b == generated3;
      }) != new_branches.end());
}

TEST(BranchGenerator, GenerateCorrectBranchesOnUpdate) {
  BranchGenerator generator;
  SpecStore store;
  std::vector<proto::Branch> generated_branches;

  proto::Branch a1 = _testing_branch({{{"1"}, {"1", ""}}});

  generator.AddPending(a1);
  generator.GenerateBranches(a1, a1.txn().ops(0).type(), a1.txn().ops(0).key(),
      store, generated_branches);

  for (const auto &branch : generated_branches) {
    PrintBranch(branch, std::cerr);
    std::cerr << std::endl;
  }
  std::cerr << std::endl;

  proto::Branch a2(a1);
  *a2.mutable_txn()->add_ops() = _testing_op({"2", ""});

  generator.AddPending(a2);
  generated_branches.clear();
  generator.GenerateBranches(a2, a2.txn().ops(1).type(), a2.txn().ops(1).key(),
      store, generated_branches);

  for (const auto &branch : generated_branches) {
    PrintBranch(branch, std::cerr);
    std::cerr << std::endl;
  }
  std::cerr << std::endl;

  proto::Branch b1 = _testing_branch({{{"2"}, {"1", "val1"}}});
  
  generator.AddPending(b1);
  generated_branches.clear();
  generator.GenerateBranches(b1, b1.txn().ops(0).type(), b1.txn().ops(0).key(),
      store, generated_branches);

  for (const auto &branch : generated_branches) {
    PrintBranch(branch, std::cerr);
    std::cerr << std::endl;
  }
  std::cerr << std::endl;

  proto::Branch a3 = _testing_branch({{{"2"}, {"1", "val1"}}, {{"1"}, {"1", ""}, {"2", ""}}});
  
  generator.AddPending(a3);
  generated_branches.clear();
  generator.GenerateBranches(a3, a3.txn().ops(1).type(), a3.txn().ops(1).key(),
      store, generated_branches);

  for (const auto &branch : generated_branches) {
    PrintBranch(branch, std::cerr);
    std::cerr << std::endl;
  }
  std::cerr << std::endl;


  proto::Branch b2(b1);
  *b2.mutable_txn()->add_ops() = _testing_op({"2", "val2"});
  
  generator.AddPending(b2);
  generated_branches.clear();
  generator.GenerateBranches(b2, b2.txn().ops(1).type(), b2.txn().ops(1).key(),
      store, generated_branches);

  for (const auto &branch : generated_branches) {
    PrintBranch(branch, std::cerr);
    std::cerr << std::endl;
  }
}

}
