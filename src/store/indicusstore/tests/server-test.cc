#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <google/protobuf/util/message_differencer.h>

#include "lib/assert.h"
#include "lib/transport.h"
#include "store/indicusstore/server.h"
#include "store/indicusstore/tests/common.h"

#define F 1
#define G 3

namespace indicusstore {

class MockTransportAddress : public TransportAddress {
 public:
  MOCK_METHOD(TransportAddress *, clone, (), (const, override));
};

class MockTransport : public Transport {
 public:
  MOCK_METHOD(void, Register, (TransportReceiver *receiver,
        const transport::Configuration &config, int groupIdx, int replicaIdx),
      (override));
  MOCK_METHOD(bool, SendMessage, (TransportReceiver *src,
        const TransportAddress &dst, const Message &m), (override));
  MOCK_METHOD(bool, SendMessageToReplica, (TransportReceiver *src,
        int replicaIdx, const Message &m), (override));
  MOCK_METHOD(bool, SendMessageToReplica, (TransportReceiver *src, int groupIdx,
        int replicaIdx, const Message &m), (override));
  MOCK_METHOD(bool, SendMessageToAll, (TransportReceiver *src,
        const Message &m), (override));
  MOCK_METHOD(bool, SendMessageToAllGroups, (TransportReceiver *src,
        const Message &m), (override));
  MOCK_METHOD(bool, SendMessageToGroups, (TransportReceiver *src,
        const std::vector<int> &groups, const Message &m), (override));
  MOCK_METHOD(bool, SendMessageToGroup, (TransportReceiver *src, int groupIdx,
        const Message &m), (override));
  MOCK_METHOD(bool, OrderedMulticast, (TransportReceiver *src,
        const std::vector<int> &groups, const Message &m), (override));
  MOCK_METHOD(bool, OrderedMulticast, (TransportReceiver *src, const Message &m),
      (override));
  MOCK_METHOD(bool, SendMessageToFC, (TransportReceiver *src, const Message &m),
      (override));
  MOCK_METHOD(int, Timer, (uint64_t ms, timer_callback_t cb), (override));
  MOCK_METHOD(bool, CancelTimer, (int id), (override));
  MOCK_METHOD(void, CancelAllTimers, (), (override));
  MOCK_METHOD(void, Run, (), (override));
  MOCK_METHOD(void, Stop, (), (override));
};

class ServerTest : public ::testing::Test {
 public:
  ServerTest() { }
  virtual ~ServerTest() { }

  virtual void SetUp() {
    int groupIdx = 0;
    int idx = 0;
    bool signedMessages = false;
    bool validateProofs = false;
    uint64_t timeDelta = 100UL;
    OCCType occType = MVTSO;

    std::stringstream configSS;
    GenerateTestConfig(1, F, configSS);
    config = new transport::Configuration(configSS);
    transport = new MockTransport();
    keyManager = new KeyManager("./");
    server = new Server(*config, groupIdx, idx, transport, keyManager,
      signedMessages, validateProofs, timeDelta, occType);
  }

  virtual void TearDown() {
    delete server;
    delete keyManager;
    delete transport;
    delete config;
  }

 protected:
  void HandleRead(const TransportAddress &remote, const proto::Read &msg) {
    server->HandleRead(remote, msg);
  }

  void HandlePhase1(const TransportAddress &remote, const proto::Phase1 &msg) {
    server->HandlePhase1(remote, msg);
  }

  void Prepare(const proto::Transaction &txn) {
    std::string txnDigest = TransactionDigest(txn);
    server->Prepare(txnDigest, txn);
  }

  void Commit(const proto::Transaction &txn) {
    std::string txnDigest = TransactionDigest(txn);
    server->Commit(txnDigest, txn);
  }

  MockTransportAddress clientAddress;
  MockTransport *transport;
  Server *server;

 private:
  transport::Configuration *config;
  KeyManager *keyManager;

};


MATCHER_P(ExpectedMessage, expected, "") {
  return google::protobuf::util::MessageDifferencer::Equals(arg, expected);
}

TEST_F(ServerTest, ReadNoData) {
  proto::Read read;
  read.set_req_id(3);
  read.set_key("key0");
  Timestamp timestamp(100, 2);
  timestamp.serialize(read.mutable_timestamp());

  proto::ReadReply expectedReply;
  expectedReply.set_req_id(3);
  expectedReply.set_status(REPLY_FAIL);
  expectedReply.set_key("key0");

  EXPECT_CALL(*transport, SendMessage(server, ::testing::_,
        ExpectedMessage(expectedReply)));

  HandleRead(clientAddress, read);
}

TEST_F(ServerTest, ReadCommittedData) {
  proto::Transaction txn;
  txn.set_client_id(1);
  txn.set_client_seq_num(2);
  WriteMessage *write = txn.add_write_set();
  write->set_key("key0");
  write->set_value("val0");
  Timestamp wts(50, 1);
  wts.serialize(txn.mutable_timestamp());
  Commit(txn);

  proto::Read read;
  read.set_req_id(3);
  read.set_key("key0");
  Timestamp timestamp(100, 2);
  timestamp.serialize(read.mutable_timestamp());

  proto::ReadReply expectedReply;
  expectedReply.set_req_id(3);
  expectedReply.set_status(REPLY_OK);
  expectedReply.set_key("key0");
  expectedReply.set_committed_value("val0");
  wts.serialize(expectedReply.mutable_committed_timestamp());

  EXPECT_CALL(*transport, SendMessage(server, ::testing::_,
        ExpectedMessage(expectedReply)));

  HandleRead(clientAddress, read);
}

TEST_F(ServerTest, ReadPreparedData) {
  proto::Transaction txn;
  WriteMessage *write = txn.add_write_set();
  write->set_key("key0");
  write->set_value("val0");
  Timestamp wts(50, 1);
  wts.serialize(txn.mutable_timestamp());
  Prepare(txn);

  proto::Read read;
  read.set_req_id(3);
  read.set_key("key0");
  Timestamp timestamp(100, 2);
  timestamp.serialize(read.mutable_timestamp());

  proto::ReadReply expectedReply;
  expectedReply.set_req_id(3);
  expectedReply.set_status(REPLY_OK);
  expectedReply.set_key("key0");
  expectedReply.mutable_prepared()->set_value("val0");
  wts.serialize(expectedReply.mutable_prepared()->mutable_timestamp());
  *expectedReply.mutable_prepared()->mutable_txn() = txn;

  EXPECT_CALL(*transport, SendMessage(server, ::testing::_,
        ExpectedMessage(expectedReply)));

  HandleRead(clientAddress, read);
}

TEST_F(ServerTest, Phase1Commit) {
  proto::Transaction txn;
  WriteMessage *write = txn.add_write_set();
  write->set_key("key0");
  write->set_value("val0");
  Timestamp wts(50, 1);
  wts.serialize(txn.mutable_timestamp());

  proto::Phase1 phase1;
  phase1.set_req_id(3);
  phase1.set_txn_digest(TransactionDigest(txn));
  *phase1.mutable_txn() = txn;

  proto::Phase1Reply expectedReply;
  expectedReply.set_req_id(3);
  expectedReply.set_status(REPLY_OK);
  expectedReply.set_ccr(proto::Phase1Reply::COMMIT);

  EXPECT_CALL(*transport, SendMessage(server, ::testing::_,
        ExpectedMessage(expectedReply)));

  HandlePhase1(clientAddress, phase1);
}

TEST_F(ServerTest, Phase1CommittedConflict) {
  proto::Transaction committedTxn;
  ReadMessage *read = committedTxn.add_read_set();
  read->set_key("key0");
  Timestamp readTime(45, 2);
  readTime.serialize(read->mutable_readtime());
  Timestamp committedRts(55, 2);
  committedRts.serialize(committedTxn.mutable_timestamp());
  Commit(committedTxn);

  proto::Transaction txn;
  WriteMessage *write = txn.add_write_set();
  write->set_key("key0");
  write->set_value("val0");
  Timestamp wts(50, 1);
  wts.serialize(txn.mutable_timestamp());

  proto::Phase1 phase1;
  phase1.set_req_id(3);
  phase1.set_txn_digest(TransactionDigest(txn));
  *phase1.mutable_txn() = txn;

  proto::Phase1Reply expectedReply;
  expectedReply.set_req_id(3);
  expectedReply.set_status(REPLY_OK);
  expectedReply.set_ccr(proto::Phase1Reply::RETRY);

  EXPECT_CALL(*transport, SendMessage(server, ::testing::_,
        ExpectedMessage(expectedReply)));

  HandlePhase1(clientAddress, phase1);
}



}
