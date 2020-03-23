#include <gtest/gtest.h>

#include <sstream>

#include "store/indicusstore/common.h"
#include "store/indicusstore/tests/common.h"
#include "lib/assert.h"

#define F 1

namespace indicusstore {

TEST(IndicusShardDecide, AllCommit) {
  std::stringstream configSS;
  GenerateTestConfig(1, F, configSS);
  transport::Configuration config(configSS);

  bool validateProofs = false;

  std::vector<proto::Phase1Reply> replies;
  proto::Phase1Reply reply;
  int n = 5 * F + 1;
  for (int i = 0; i < n; ++i) {
    reply.set_ccr(proto::Phase1Reply::COMMIT);
    replies.push_back(reply);
  }

  UW_ASSERT(replies.size() == static_cast<size_t>(5 * F + 1)); 

  bool fast;
  proto::CommitDecision decision = IndicusShardDecide(replies, &config,
      validateProofs, fast);

  EXPECT_EQ(decision, proto::COMMIT);
}

TEST(IndicusShardDecide, OneAbort) {
  std::stringstream configSS;
  GenerateTestConfig(1, F, configSS);
  transport::Configuration config(configSS);

  bool validateProofs = false;

  std::vector<proto::Phase1Reply> replies;
  proto::Phase1Reply reply;
  int n = 5 * F + 1;
  for (int i = 0; i < n - 1; ++i) {
    reply.set_ccr(proto::Phase1Reply::COMMIT);
    replies.push_back(reply);
  }
  
  reply.set_ccr(proto::Phase1Reply::ABORT);
  replies.push_back(reply);

  UW_ASSERT(replies.size() == static_cast<size_t>(5 * F + 1)); 

  bool fast;
  proto::CommitDecision decision = IndicusShardDecide(replies, &config,
      validateProofs, fast);

  EXPECT_EQ(decision, proto::ABORT);
}

TEST(IndicusShardDecide, AbstainCommit) {
  std::stringstream configSS;
  GenerateTestConfig(1, F, configSS);
  transport::Configuration config(configSS);

  bool validateProofs = false;

  std::vector<proto::Phase1Reply> replies;
  proto::Phase1Reply reply;
  for (int i = 0; i < 3 * F + 1; ++i) {
    reply.set_ccr(proto::Phase1Reply::COMMIT);
    replies.push_back(reply);
  }
  for (int i = 0; i < 2 * F; ++i) {
    reply.set_ccr(proto::Phase1Reply::ABSTAIN);
    replies.push_back(reply);
  }

  UW_ASSERT(replies.size() == static_cast<size_t>(5 * F + 1)); 

  bool fast;
  proto::CommitDecision decision = IndicusShardDecide(replies, &config,
      validateProofs, fast);

  EXPECT_EQ(decision, proto::ABORT);
}

TEST(IndicusShardDecide, AbstainAbort) {
  std::stringstream configSS;
  GenerateTestConfig(1, F, configSS);
  transport::Configuration config(configSS);

  bool validateProofs = false;

  std::vector<proto::Phase1Reply> replies;
  proto::Phase1Reply reply;
  for (int i = 0; i < 3 * F + 1; ++i) {
    reply.set_ccr(proto::Phase1Reply::ABSTAIN);
    replies.push_back(reply);
  }
  for (int i = 0; i < 2 * F; ++i) {
    reply.set_ccr(proto::Phase1Reply::COMMIT);
    replies.push_back(reply);
  }

  UW_ASSERT(replies.size() == static_cast<size_t>(5 * F + 1)); 

  bool fast;
  proto::CommitDecision decision = IndicusShardDecide(replies, &config,
      validateProofs, fast);

  EXPECT_EQ(decision, proto::ABORT);
}

TEST(IndicusDecide, AllCommit) {
  int g = 2;

  std::stringstream configSS;
  GenerateTestConfig(g, F, configSS);
  transport::Configuration config(configSS);

  bool validateProofs = false;

  std::map<int, std::vector<proto::Phase1Reply>> replies;
  proto::Phase1Reply reply;
  int n = 5 * F + 1;
  for (int i = 0; i < g; ++i) {
    for (int j = 0; j < n; ++j) {
      reply.set_ccr(proto::Phase1Reply::COMMIT);
      replies[i].push_back(reply);
    }
    UW_ASSERT(replies[i].size() == static_cast<size_t>(5 * F + 1)); 
  }

  proto::CommitDecision decision = IndicusDecide(replies, &config,
      validateProofs);

  EXPECT_EQ(decision, proto::COMMIT);
}

TEST(IndicusDecide, OneAbort) {
  int g = 2;

  std::stringstream configSS;
  GenerateTestConfig(g, F, configSS);
  transport::Configuration config(configSS);

  bool validateProofs = false;

  std::map<int, std::vector<proto::Phase1Reply>> replies;
  proto::Phase1Reply reply;
  int n = 5 * F + 1;
  for (int i = 0; i < g; ++i) {
    int commits;
    if (i == 1) {
      reply.set_ccr(proto::Phase1Reply::ABORT);
      replies[i].push_back(reply);
      commits = n - 1;
    } else {
      commits = n;
    }
    for (int j = 0; j < commits; ++j) {
      reply.set_ccr(proto::Phase1Reply::COMMIT);
      replies[i].push_back(reply);
    }
    UW_ASSERT(replies[i].size() == static_cast<size_t>(5 * F + 1)); 
  }

  proto::CommitDecision decision = IndicusDecide(replies, &config,
      validateProofs);
  EXPECT_EQ(decision, proto::ABORT);
}

}
