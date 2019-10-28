#include "store/janusstore/transaction.h"
#include "store/janusstore/server.h"
#include "store/janusstore/client.h"
#include "store/common/stats.h"
#include "lib/simtransport.h"

#include <gtest/gtest.h>
#include <stdio.h>

using namespace transport;
using std::vector;
using std::map;

// class TestReceiver : public TransportReceiver
// {
// public:
//     TestReceiver();
//     void ReceiveMessage(const TransportAddress &src,
//                         const string &type, const string &data);

//     int numReceived;
//     TestMessage lastMsg;
// };

// TestReceiver::TestReceiver()
// {
//     numReceived = 0;
// }

// void
// TestReceiver::ReceiveMessage(const TransportAddress &src,
//                              const string &type, const string &data)
// {
//     UW_ASSERT_EQ(type, lastMsg.GetTypeName());
//     lastMsg.ParseFromString(data);
//     numReceived++;
// }

class JanusServerTest : public  ::testing::Test
{
protected:
    vector<ReplicaAddress> replicaAddrs;
    map<int, std::vector<ReplicaAddress>> *g_replicas;
    std::unique_ptr<transport::Configuration> config;
    SimulatedTransport *transport;

    janusstore::Server *server;
    janusstore::Client* client;
    std::vector<janusstore::Server*> replicas;

    int shards;
    int replicas_per_shard;

    JanusServerTest() : shards(1), replicas_per_shard(3) {
        replicaAddrs = {
            { "localhost", "12345" },
            { "localhost", "12346" },
            { "localhost", "12347" }
        };
    }

    virtual void SetUp() {
        g_replicas = new std::map<int, std::vector<ReplicaAddress>>({{ 0, replicaAddrs }});

        config = std::unique_ptr<transport::Configuration>(
            new transport::Configuration(
                shards, replicas_per_shard, 1, *g_replicas));

        transport = new SimulatedTransport();

        for (int i = 0; i < replicas_per_shard; i++) {
            auto p = new janusstore::Server(*config, 0, i, transport);
            replicas.push_back(std::move(p));
        }
        server = replicas.front();
        // TODO init client
    };

    virtual int ServerGroup() {
        return server->groupIdx;
    }

    virtual int ServerId() {
        return server->myIdx;
    }

    virtual int Shards() {
        return shards;
    }

    virtual int ReplicasPerShard() {
        return replicas.size();
    }
};

TEST_F(JanusServerTest, Init)
{
    EXPECT_EQ(ServerGroup(), 0);
    EXPECT_EQ(ServerId(), 0);
}

TEST_F(JanusServerTest, BuildDepListNoDeps)
{
    janusstore::Transaction txn1(1234);
    janusstore::Transaction* txn_ptr1 = &txn1;

    std::vector<uint64_t> *result = server->BuildDepList(txn1, 0);
    EXPECT_EQ(result->size(), 0);
}

TEST_F(JanusServerTest, BuildDepListNoConflict)
{
    janusstore::Transaction txn1(1234);
    janusstore::Transaction* txn_ptr1 = &txn1;
    txn_ptr1->addReadSet("key1");
    txn_ptr1->addWriteSet("key2", "val2");

    janusstore::Transaction txn2(1235);
    janusstore::Transaction* txn_ptr2 = &txn2;
    txn_ptr2->addReadSet("key3");
    txn_ptr2->addWriteSet("key4", "val4");

    std::vector<uint64_t> *result = server->BuildDepList(txn1, 0);
    EXPECT_EQ(result->size(), 0);

    result = server->BuildDepList(txn2, 1);
    EXPECT_EQ(result->size(), 0);
}

TEST_F(JanusServerTest, BuildDepListSingleConflict)
{
    janusstore::Transaction txn1(1234);
    janusstore::Transaction* txn_ptr1 = &txn1;
    txn_ptr1->addReadSet("key1");
    txn_ptr1->addWriteSet("key2", "val2");

    janusstore::Transaction txn2(1235);
    janusstore::Transaction* txn_ptr2 = &txn2;
    txn_ptr2->addReadSet("key1");
    txn_ptr2->addWriteSet("key2", "val3");

    server->BuildDepList(txn1, 0);

    std::vector<uint64_t> *result = server->BuildDepList(txn2, 1);
    EXPECT_EQ(result->size(), 1);
    EXPECT_EQ(result->at(0), 1234);
}

TEST_F(JanusServerTest, BuildDepListMultipleConflicts)
{
    janusstore::Transaction txn1(1234);
    janusstore::Transaction* txn_ptr1 = &txn1;
    txn_ptr1->addReadSet("key1");
    txn_ptr1->addWriteSet("key2", "val2");

    janusstore::Transaction txn2(1235);
    janusstore::Transaction* txn_ptr2 = &txn2;
    txn_ptr2->addReadSet("key1");
    txn_ptr2->addWriteSet("key2", "val3");

    janusstore::Transaction txn3(4000);
    janusstore::Transaction* txn_ptr3 = &txn3;
    txn_ptr3->addReadSet("key2");

    server->BuildDepList(txn1, 0);
    server->BuildDepList(txn2, 0);
    std::vector<uint64_t> *result = server->BuildDepList(txn3, 0);
    EXPECT_EQ(result->size(), 2);
    EXPECT_TRUE(std::find(result->begin(), result->end(), 1234) != result->end());
    EXPECT_TRUE(std::find(result->begin(), result->end(), 1235) != result->end());
}

TEST_F(JanusServerTest, BuildDepListRejectBallot)
{
    janusstore::Transaction txn1(1234);
    janusstore::Transaction* txn_ptr1 = &txn1;
    txn_ptr1->addReadSet("key1");
    txn_ptr1->addWriteSet("key2", "val2");

    server->BuildDepList(txn1, 0);
    std::vector<uint64_t> *result = server->BuildDepList(txn1, 1);
    EXPECT_EQ(result, nullptr);
}

TEST_F(JanusServerTest, SCCNoCycles)
{
    std::unordered_map<uint64_t, std::vector<uint64_t>> sample_dep_map {
        { 1234, { 4567 }},
        { 4567, { 7890 }},
        { 7890, { 1111 }},
        { 1111, {}}
    };
    server->dep_map = sample_dep_map;
    vector<uint64_t> scc = server->_StronglyConnectedComponent(1234);
    EXPECT_EQ(scc.size(), 1);
    EXPECT_EQ(scc[0], 1234);
    scc = server->_StronglyConnectedComponent(4567);
    EXPECT_EQ(scc.size(), 1);
    EXPECT_EQ(scc[0], 4567);
    scc = server->_StronglyConnectedComponent(7890);
    EXPECT_EQ(scc.size(), 1);
    EXPECT_EQ(scc[0], 7890);
}

TEST_F(JanusServerTest, SCCSimpleCycle)
{
    std::unordered_map<uint64_t, std::vector<uint64_t>> sample_dep_map {
        { 1234, { 4567 }},
        { 4567, { 7890 }},
        { 7890, { 1234 }}
    };
    server->dep_map = sample_dep_map;
    vector<uint64_t> scc = server->_StronglyConnectedComponent(1234);
    EXPECT_EQ(scc.size(), 3);
    scc = server->_StronglyConnectedComponent(4567);
    EXPECT_EQ(scc.size(), 3);
    scc = server->_StronglyConnectedComponent(7890);
    EXPECT_EQ(scc.size(), 3);
}

TEST_F(JanusServerTest, SCCPartialCycle)
{
    std::unordered_map<uint64_t, std::vector<uint64_t>> sample_dep_map {
        { 1234, { 4567 }},
        { 4567, { 7890 }},
        { 7890, { 1111 }},
        { 1111, { 9999, 2222 }},
        { 9999, { 4567, 3333 }},
        { 3333, { }},
        { 2222, { }}
    };
    server->dep_map = sample_dep_map;
    vector<uint64_t> scc = server->_StronglyConnectedComponent(1234);
    EXPECT_EQ(scc.size(), 1);
    scc = server->_StronglyConnectedComponent(4567);
    for (auto id : scc) {
        Debug("%i", id);
    }
    EXPECT_EQ(scc.size(), 4);
    scc = server->_StronglyConnectedComponent(9999);
    for (auto id : scc) {
        Debug("%id", id);
    }
    EXPECT_EQ(scc.size(), 4);
    scc = server->_StronglyConnectedComponent(2222);
    EXPECT_EQ(scc.size(), 1);
}

TEST_F(JanusServerTest, SCCMultipleCycles)
{
    std::unordered_map<uint64_t, std::vector<uint64_t>> sample_dep_map {
        { 1234, { 4567 }},
        { 4567, { 7890 }},
        { 7890, { 1111 }},
        { 1111, { 9999, 2222 }},
        { 9999, { 4567, 3333 }},
        { 3333, { }},
        { 2222, { 0000 }},
        { 0000, { 4444 }},
        { 4444, { 2222 }}
    };
    server->dep_map = sample_dep_map;
    vector<uint64_t> scc = server->_StronglyConnectedComponent(1234);
    EXPECT_EQ(scc.size(), 1);
    scc = server->_StronglyConnectedComponent(4567);
    EXPECT_EQ(scc.size(), 4);
    scc = server->_StronglyConnectedComponent(9999);
    EXPECT_EQ(scc.size(), 4);
    scc = server->_StronglyConnectedComponent(2222);
    EXPECT_EQ(scc.size(), 3);
    scc = server->_StronglyConnectedComponent(0000);
    EXPECT_EQ(scc.size(), 3);
}
