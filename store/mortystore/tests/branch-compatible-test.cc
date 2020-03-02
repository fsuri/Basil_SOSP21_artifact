#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "lib/configuration.h"
#include "store/mortystore/common.h"
#include "lib/assert.h"

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
  proto::Branch branch = _testing_branch( 
    {
      {
        {"3"}, {"0", ""}
      },
      {
        {"4"}, {"28", ""}, {"28", "val4"}, {"0", ""}, {"0", "val0"}
      }
    });
  std::vector<proto::Transaction> committed = _testing_txns({
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
