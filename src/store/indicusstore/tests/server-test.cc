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

  MockTransportAddress clientAddress;
  MockTransport *transport;
  Server *server;

 private:
  transport::Configuration *config;
  KeyManager *keyManager;

};


MATCHER(ExpectedReadReply, "") {
  proto::ReadReply expectedReply;
  expectedReply.set_req_id(3);
  expectedReply.set_status(REPLY_FAIL);
  expectedReply.set_key("key0");
  return google::protobuf::util::MessageDifferencer::Equals(arg, expectedReply);
}

TEST_F(ServerTest, ReadNoData) {
  proto::Read read;
  read.set_req_id(3);
  read.set_txn_id(4);
  read.set_key("key0");
  Timestamp timestamp(100, 2);
  timestamp.serialize(read.mutable_timestamp());

  EXPECT_CALL(*transport, SendMessage(server, ::testing::_, ExpectedReadReply()));

  HandleRead(clientAddress, read);
}

}
