#include <gtest/gtest.h>

#include <sstream>
#include <tuple>

#include "store/indicusstore/common.h"
#include "store/indicusstore/tests/common.h"
#include "lib/assert.h"

#define F 1

namespace indicusstore {

class CommonTest : public ::testing::TestWithParam<std::tuple<int, int, bool>> {
 public:
  CommonTest() { }
  virtual ~CommonTest() { }

  virtual void SetUp() {
    std::stringstream configSS;
    GenerateTestConfig(GetNumGroups(), GetF(), configSS);
    config = new transport::Configuration(configSS);
    n = 5 * GetF() + 1;
  }

  virtual void TearDown() {
    delete config;
  }

 protected:
  inline int GetNumGroups() const { return std::get<0>(GetParam()); }
  inline int GetF() const { return std::get<1>(GetParam()); }
  inline int IsValidatingProofs() const { return std::get<2>(GetParam()); }
  inline int GetN() const { return n; }

  transport::Configuration *config;

 private:
  int n;
};

TEST_P(CommonTest, IndicusShardDecideAllCommit) {
  std::vector<proto::Phase1Reply> replies;
  proto::Phase1Reply reply;
  for (int i = 0; i < GetN(); ++i) {
    reply.set_ccr(proto::Phase1Reply::COMMIT);
    replies.push_back(reply);
  }

  UW_ASSERT(replies.size() == static_cast<size_t>(GetN())); 

  bool fast;
  proto::CommitDecision decision = IndicusShardDecide(replies, config,
      IsValidatingProofs(), false, nullptr, fast);

  EXPECT_EQ(decision, proto::COMMIT);
}

TEST_P(CommonTest, IndicusShardDecideOneAbort) {
  std::vector<proto::Phase1Reply> replies;
  proto::Phase1Reply reply;
  for (int i = 0; i < GetN() - 1; ++i) {
    reply.set_ccr(proto::Phase1Reply::COMMIT);
    replies.push_back(reply);
  }
  
  reply.set_ccr(proto::Phase1Reply::ABORT);
  if (IsValidatingProofs()) {
    PopulateCommitProof(*reply.mutable_committed_conflict(), GetN());
  }
  replies.push_back(reply);

  UW_ASSERT(replies.size() == static_cast<size_t>(GetN())); 

  bool fast;
  proto::CommitDecision decision = IndicusShardDecide(replies, config,
      IsValidatingProofs(), false, nullptr, fast);

  EXPECT_EQ(decision, proto::ABORT);
}

TEST_P(CommonTest, IndicusShardDecideAbstainCommit) {
  std::vector<proto::Phase1Reply> replies;
  proto::Phase1Reply reply;
  for (int i = 0; i < 3 * GetF() + 1; ++i) {
    reply.set_ccr(proto::Phase1Reply::COMMIT);
    replies.push_back(reply);
  }
  for (int i = 0; i < 2 * GetF(); ++i) {
    reply.set_ccr(proto::Phase1Reply::ABSTAIN);
    replies.push_back(reply);
  }

  UW_ASSERT(replies.size() == static_cast<size_t>(GetN())); 

  bool fast;
  proto::CommitDecision decision = IndicusShardDecide(replies, config,
      IsValidatingProofs(), false, nullptr, fast);

  EXPECT_EQ(decision, proto::COMMIT);
}

TEST_P(CommonTest, IndicusShardDecideAbstainAbort) {
  std::vector<proto::Phase1Reply> replies;
  proto::Phase1Reply reply;
  for (int i = 0; i < 3 * GetF() + 1; ++i) {
    reply.set_ccr(proto::Phase1Reply::ABSTAIN);
    replies.push_back(reply);
  }
  for (int i = 0; i < 2 * GetF(); ++i) {
    reply.set_ccr(proto::Phase1Reply::COMMIT);
    replies.push_back(reply);
  }

  UW_ASSERT(replies.size() == static_cast<size_t>(GetN())); 

  bool fast;
  proto::CommitDecision decision = IndicusShardDecide(replies, config,
      IsValidatingProofs(), false, nullptr, fast);

  EXPECT_EQ(decision, proto::ABORT);
}

TEST_P(CommonTest, IndicusDecideAllCommit) {
  std::map<int, std::vector<proto::Phase1Reply>> replies;
  proto::Phase1Reply reply;
  for (int i = 0; i < GetNumGroups(); ++i) {
    for (int j = 0; j < GetN(); ++j) {
      reply.set_ccr(proto::Phase1Reply::COMMIT);
      replies[i].push_back(reply);
    }
    UW_ASSERT(replies[i].size() == static_cast<size_t>(GetN())); 
  }

  proto::CommitDecision decision = IndicusDecide(replies, config,
      IsValidatingProofs(), false, nullptr);

  EXPECT_EQ(decision, proto::COMMIT);
}

TEST_P(CommonTest, IndicusDecideOneAbort) {
  std::map<int, std::vector<proto::Phase1Reply>> replies;
  proto::Phase1Reply reply;
  for (int i = 0; i < GetNumGroups(); ++i) {
    int commits;
    if (i == GetNumGroups() / 2) {
      reply.set_ccr(proto::Phase1Reply::ABORT);
      if (IsValidatingProofs()) {
        PopulateCommitProof(*reply.mutable_committed_conflict(), GetN());
      }
      replies[i].push_back(reply);
      commits = GetN() - 1;
    } else {
      commits = GetN();
    }
    for (int j = 0; j < commits; ++j) {
      reply.set_ccr(proto::Phase1Reply::COMMIT);
      replies[i].push_back(reply);
    }
    UW_ASSERT(replies[i].size() == static_cast<size_t>(GetN())); 
  }

  proto::CommitDecision decision = IndicusDecide(replies, config,
      IsValidatingProofs(), false, nullptr);
  EXPECT_EQ(decision, proto::ABORT);
}

TEST_P(CommonTest, ValidateProofValidFastPath) {
  std::set<int> involvedGroups;
  for (int i = 0; i < GetNumGroups(); ++i) {
    involvedGroups.insert(i);
  }
  proto::Transaction txn;
  PopulateTransaction({}, {{"key0", "val0"}, {"key2", "val2"}}, Timestamp(1,1),
      involvedGroups, txn);
  std::string txnDigest = TransactionDigest(txn);

  proto::GroupedPhase1Replies replies;
  proto::Phase1Reply reply;
  for (int i = 0; i < GetNumGroups(); ++i) {
    for (int j = 0; j < GetN(); ++j) {
      reply.set_ccr(proto::Phase1Reply::COMMIT);
      *reply.mutable_txn_digest() = txnDigest;
      *(*replies.mutable_replies())[i].add_replies() = reply;
    }
  }

  proto::CommittedProof proof;
  *proof.mutable_txn() = txn;
  *proof.mutable_p1_replies() = replies;

  EXPECT_TRUE(ValidateProof(proof, config, false, nullptr));
}

TEST_P(CommonTest, ValidateProofInvalidFastPathDigest) {
  std::set<int> involvedGroups;
  for (int i = 0; i < GetNumGroups(); ++i) {
    involvedGroups.insert(i);
  }
  proto::Transaction txn;
  PopulateTransaction({}, {{"key0", "val0"}, {"key2", "val2"}}, Timestamp(1, 1),
      involvedGroups, txn);
  std::string txnDigest = TransactionDigest(txn);

  proto::Transaction txn2;
  PopulateTransaction({}, {{"key0", "val1"}, {"key1", "val1"}}, Timestamp(2, 2),
      involvedGroups, txn2);
  std::string txn2Digest = TransactionDigest(txn2);

  proto::GroupedPhase1Replies replies;
  proto::Phase1Reply reply;
  for (int i = 0; i < GetNumGroups(); ++i) {
    for (int j = 0; j < GetN(); ++j) {
      reply.set_ccr(proto::Phase1Reply::COMMIT);
      if (i == GetNumGroups() / 2 && j == GetN() / 2) {
        // have one replica respond with Phase1Reply for different transaction
        *reply.mutable_txn_digest() = txn2Digest;
      } else {
        *reply.mutable_txn_digest() = txnDigest;
      }
      *(*replies.mutable_replies())[i].add_replies() = reply;
    }
  }

  proto::CommittedProof proof;
  *proof.mutable_txn() = txn;
  *proof.mutable_p1_replies() = replies;

  EXPECT_FALSE(ValidateProof(proof, config, false, nullptr));
}

TEST_P(CommonTest, ValidateProofInvalidFastPathMissingReplies) {
  std::set<int> involvedGroups;
  for (int i = 0; i < GetNumGroups(); ++i) {
    involvedGroups.insert(i);
  }
  proto::Transaction txn;
  PopulateTransaction({}, {{"key0", "val0"}, {"key2", "val2"}}, Timestamp(1,1),
      involvedGroups, txn);
  std::string txnDigest = TransactionDigest(txn);

  proto::GroupedPhase1Replies replies;
  proto::Phase1Reply reply;
  for (int i = 0; i < GetNumGroups(); ++i) {
    int m = (i == GetNumGroups() / 2) ? GetN() - 1 : GetN();
    for (int j = 0; j < m; ++j) {
      reply.set_ccr(proto::Phase1Reply::COMMIT);
      *reply.mutable_txn_digest() = txnDigest;
      *(*replies.mutable_replies())[i].add_replies() = reply;
    }
  }

  proto::CommittedProof proof;
  *proof.mutable_txn() = txn;
  *proof.mutable_p1_replies() = replies;

  EXPECT_FALSE(ValidateProof(proof, config, false, nullptr));
}

TEST_P(CommonTest, ValidateProofValidSlowPath) {
  std::set<int> involvedGroups;
  for (int i = 0; i < GetNumGroups(); ++i) {
    involvedGroups.insert(i);
  }
  proto::Transaction txn;
  PopulateTransaction({}, {{"key0", "val0"}, {"key2", "val2"}}, Timestamp(1,1),
      involvedGroups, txn);
  std::string txnDigest = TransactionDigest(txn);

  proto::Phase2Replies replies;
  proto::Phase2Reply reply;
  int m = 4 * GetF() + 1;
  for (int j = 0; j < m; ++j) {
    reply.set_decision(proto::COMMIT);
    *reply.mutable_txn_digest() = txnDigest;
    *replies.add_replies() = reply;
  }

  proto::CommittedProof proof;
  *proof.mutable_txn() = txn;
  *proof.mutable_p2_replies() = replies;

  EXPECT_TRUE(ValidateProof(proof, config, false, nullptr));
}

TEST_P(CommonTest, ValidateProofInvalidSlowPathDigest) {
  std::set<int> involvedGroups;
  for (int i = 0; i < GetNumGroups(); ++i) {
    involvedGroups.insert(i);
  }
  proto::Transaction txn;
  PopulateTransaction({}, {{"key0", "val0"}, {"key2", "val2"}}, Timestamp(1,1),
      involvedGroups, txn);
  std::string txnDigest = TransactionDigest(txn);

  proto::Transaction txn2;
  PopulateTransaction({}, {{"key0", "val1"}, {"key1", "val1"}}, Timestamp(2, 2),
      involvedGroups, txn2);
  std::string txn2Digest = TransactionDigest(txn2);

  proto::Phase2Replies replies;
  proto::Phase2Reply reply;
  int m = 4 * GetF() + 1;
  for (int j = 0; j < m; ++j) {
    reply.set_decision(proto::COMMIT);
    if (j == m / 2) {
      // have one replica respond with Phase1Reply for different transaction
      *reply.mutable_txn_digest() = txn2Digest;
    } else {
      *reply.mutable_txn_digest() = txnDigest;
    }
    *replies.add_replies() = reply;
  }

  proto::CommittedProof proof;
  *proof.mutable_txn() = txn;
  *proof.mutable_p2_replies() = replies;

  EXPECT_FALSE(ValidateProof(proof, config, false, nullptr));
}

TEST_P(CommonTest, ValidateProofInvalidSlowPathMissingReplies) {
  std::set<int> involvedGroups;
  for (int i = 0; i < GetNumGroups(); ++i) {
    involvedGroups.insert(i);
  }
  proto::Transaction txn;
  PopulateTransaction({}, {{"key0", "val0"}, {"key2", "val2"}}, Timestamp(1,1),
      involvedGroups, txn);
  std::string txnDigest = TransactionDigest(txn);

  proto::Phase2Replies replies;
  proto::Phase2Reply reply;
  int m = 4 * GetF();
  for (int j = 0; j < m; ++j) {
    reply.set_decision(proto::COMMIT);
    *reply.mutable_txn_digest() = txnDigest;
    *replies.add_replies() = reply;
  }

  proto::CommittedProof proof;
  *proof.mutable_txn() = txn;
  *proof.mutable_p2_replies() = replies;

  EXPECT_FALSE(ValidateProof(proof, config, false, nullptr));
}

TEST_P(CommonTest, ValidateTransactionWriteValidFastPath) {
  std::set<int> involvedGroups;
  for (int i = 0; i < GetNumGroups(); ++i) {
    involvedGroups.insert(i);
  }
  proto::Transaction txn;
  PopulateTransaction({}, {{"key0", "val0"}, {"key2", "val2"}}, Timestamp(1,1),
      involvedGroups, txn);
  std::string txnDigest = TransactionDigest(txn);

  proto::GroupedPhase1Replies replies;
  proto::Phase1Reply reply;
  for (int i = 0; i < GetNumGroups(); ++i) {
    for (int j = 0; j < GetN(); ++j) {
      reply.set_ccr(proto::Phase1Reply::COMMIT);
      *reply.mutable_txn_digest() = txnDigest;
      *(*replies.mutable_replies())[i].add_replies() = reply;
    }
  }

  proto::CommittedProof proof;
  *proof.mutable_txn() = txn;
  *proof.mutable_p1_replies() = replies;

  EXPECT_TRUE(ValidateTransactionWrite(proof, "key0", "val0", Timestamp(1, 1),
        config, false, nullptr));
}

TEST_P(CommonTest, ValidateTransactionWriteInvalidWrongValue) {
  std::set<int> involvedGroups;
  for (int i = 0; i < GetNumGroups(); ++i) {
    involvedGroups.insert(i);
  }
  proto::Transaction txn;
  PopulateTransaction({}, {{"key0", "val0"}, {"key2", "val2"}}, Timestamp(1,1),
      involvedGroups, txn);
  std::string txnDigest = TransactionDigest(txn);

  proto::GroupedPhase1Replies replies;
  proto::Phase1Reply reply;
  for (int i = 0; i < GetNumGroups(); ++i) {
    for (int j = 0; j < GetN(); ++j) {
      reply.set_ccr(proto::Phase1Reply::COMMIT);
      *reply.mutable_txn_digest() = txnDigest;
      *(*replies.mutable_replies())[i].add_replies() = reply;
    }
  }

  proto::CommittedProof proof;
  *proof.mutable_txn() = txn;
  *proof.mutable_p1_replies() = replies;

  EXPECT_FALSE(ValidateTransactionWrite(proof, "key0", "val1", Timestamp(1, 1),
        config, false, nullptr));
}

TEST_P(CommonTest, ValidateTransactionWriteInvalidMissingWriteSet) {
  std::set<int> involvedGroups;
  for (int i = 0; i < GetNumGroups(); ++i) {
    involvedGroups.insert(i);
  }
  proto::Transaction txn;
  PopulateTransaction({}, {{"key0", "val0"}, {"key2", "val2"}}, Timestamp(1,1),
      involvedGroups, txn);
  std::string txnDigest = TransactionDigest(txn);

  proto::GroupedPhase1Replies replies;
  proto::Phase1Reply reply;
  for (int i = 0; i < GetNumGroups(); ++i) {
    for (int j = 0; j < GetN(); ++j) {
      reply.set_ccr(proto::Phase1Reply::COMMIT);
      *reply.mutable_txn_digest() = txnDigest;
      *(*replies.mutable_replies())[i].add_replies() = reply;
    }
  }

  proto::CommittedProof proof;
  *proof.mutable_txn() = txn;
  *proof.mutable_p1_replies() = replies;

  EXPECT_FALSE(ValidateTransactionWrite(proof, "key1", "val1", Timestamp(1, 1),
        config, false, nullptr));
}

TEST_P(CommonTest, ValidateTransactionWriteInvalidWrongTimestamp) {
  std::set<int> involvedGroups;
  for (int i = 0; i < GetNumGroups(); ++i) {
    involvedGroups.insert(i);
  }
  proto::Transaction txn;
  PopulateTransaction({}, {{"key0", "val0"}, {"key2", "val2"}}, Timestamp(1,1),
      involvedGroups, txn);
  std::string txnDigest = TransactionDigest(txn);

  proto::GroupedPhase1Replies replies;
  proto::Phase1Reply reply;
  for (int i = 0; i < GetNumGroups(); ++i) {
    for (int j = 0; j < GetN(); ++j) {
      reply.set_ccr(proto::Phase1Reply::COMMIT);
      *reply.mutable_txn_digest() = txnDigest;
      *(*replies.mutable_replies())[i].add_replies() = reply;
    }
  }

  proto::CommittedProof proof;
  *proof.mutable_txn() = txn;
  *proof.mutable_p1_replies() = replies;

  EXPECT_FALSE(ValidateTransactionWrite(proof, "key1", "val1", Timestamp(3, 2),
        config, false, nullptr));
}

INSTANTIATE_TEST_SUITE_P(CommonTests, CommonTest, ::testing::Values(
	std::make_tuple(1, 1, false), std::make_tuple(2, 1, false), std::make_tuple(3, 1, false),
	std::make_tuple(2, 2, false), std::make_tuple(3, 2, false),
  std::make_tuple(1, 1, true), std::make_tuple(2, 1, true), std::make_tuple(3, 1, true),
	std::make_tuple(2, 2, true), std::make_tuple(3, 2, true)));

}
