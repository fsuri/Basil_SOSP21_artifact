#include "store/janusstore/transaction.h"
#include "store/janusstore/client.h"
#include "store/common/stats.h"
#include "lib/simtransport.h"

#include <gtest/gtest.h>
#include <stdio.h>

using namespace transport;
using namespace janusstore::proto;
using std::vector;
using std::map;

class JanusClientTest : public  ::testing::Test
{
protected:
    vector<ReplicaAddress> replicaAddrs;
    map<int, std::vector<ReplicaAddress>> *g_replicas;
    transport::Configuration *config;
    SimulatedTransport *transport;

    janusstore::Client *client;
    janusstore::Transaction *txn;

    int shards;
    int replicas_per_shard;
    uint64_t ballot;

    JanusClientTest() : shards(1), replicas_per_shard(3) {
        replicaAddrs = {
            { "localhost", "12345" },
            { "localhost", "12346" },
            { "localhost", "12347" }
        };
    }

    virtual void SetUp() {
        g_replicas = new std::map<int, std::vector<ReplicaAddress>>({{ 0, replicaAddrs }});

        config = new transport::Configuration(
                shards, replicas_per_shard, 1, *g_replicas);

        txn = new janusstore::Transaction(1234);
        txn->addReadSet("key1");
        txn->addReadSet("key2");
        txn->addWriteSet("key3", "val3");
        txn->setTransactionStatus(janusstore::proto::TransactionMessage::PREACCEPT);

        ballot = 0;

        transport = new SimulatedTransport();
        client = new janusstore::Client(config, shards, 0, transport);
    };

    virtual janusstore::Client* Client() {
        return client;
    }

    virtual janusstore::Transaction* Transaction() {
        return txn;
    }

    virtual uint64_t Ballot() {
        return ballot;
    }
};

TEST_F(JanusClientTest, Init)
{
    EXPECT_EQ(Ballot(), 0);
    EXPECT_EQ(Transaction()->getTransactionId(),1234);
}

TEST_F(JanusClientTest, PreAccept) {

    auto ccb = [] (uint64_t committed, std::map<std::string, std::string> readValues) {
        printf("output commit from txn %d \r\n", committed);
    };

    Client()->PreAccept(Transaction(), Ballot(), ccb);

    // verify the correct metadata for the txn is set on client
    auto it = Client()->pendingReqs.find(1234);
    ASSERT_EQ(it != Client()->pendingReqs.end(), true);

    janusstore::Client::PendingRequest* req = Client()->pendingReqs.at(1234);
    EXPECT_EQ(req->txn_id,1234);
    EXPECT_EQ(req->has_fast_quorum,false);
    EXPECT_EQ(req->output_committed,false);
    EXPECT_EQ(req->participant_shards.size(),1);
    EXPECT_EQ(req->participant_shards.find(0) != req->participant_shards.end(),true);
    EXPECT_EQ(req->aggregated_deps.size(),0);
    EXPECT_EQ(req->responded_shards.size(),0);

    // verify correct metadata for the txn is set on shardclient
    EXPECT_EQ(Client()->bclient[0]->shard, 0);
    EXPECT_EQ(Client()->bclient[0]->num_replicas, 3);
    EXPECT_EQ(Client()->bclient[0]->responded, 0);
}

TEST_F(JanusClientTest, PreAcceptCallbackNoDep) {

    auto ccb = [] (uint64_t committed, std::map<std::string, std::string> readValues) {
        printf("output commit from txn %d \r\n", committed);
    };

    PreAcceptOKMessage preaccept_ok_msg;
    preaccept_ok_msg.set_txnid(1234);
    // note: no dependencies set for the preacceptok

    Reply reply;
    reply.set_op(Reply::PREACCEPT_OK);
    reply.set_allocated_preaccept_ok(&preaccept_ok_msg);

    std::vector<Reply> replies = {reply};

    Client()->PreAccept(Transaction(), Ballot(), ccb);
    Client()->PreAcceptCallback(1234, 0, replies);

    preaccept_ok_msg.release_dep();
    reply.release_preaccept_ok();

    // verify the correct metadata and behavior when a shardclient invokes the client's callback
    janusstore::Client::PendingRequest* req = Client()->pendingReqs.at(1234);
    EXPECT_EQ(req->responded_shards.size(),1);
    EXPECT_EQ(req->responded_shards.find(0) != req->responded_shards.end(),true);
    EXPECT_EQ(req->has_fast_quorum,true);
    EXPECT_EQ(req->aggregated_deps.size(),0);
}

TEST_F(JanusClientTest, PreAcceptCallbackHasDep) {

    auto ccb = [] (uint64_t committed, std::map<std::string, std::string> readValues) {
        printf("output commit from txn %d \r\n", committed);
    };

    PreAcceptOKMessage preaccept_ok_msg;
    DependencyList dep;
    dep.add_txnid(4567);
    preaccept_ok_msg.set_txnid(1234);
    // note: dependency set for the preacceptok
    preaccept_ok_msg.set_allocated_dep(&dep);

    Reply reply;
    reply.set_op(Reply::PREACCEPT_OK);
    reply.set_allocated_preaccept_ok(&preaccept_ok_msg);

    std::vector<Reply> replies = {reply};

    Client()->PreAccept(Transaction(), Ballot(), ccb);
    Client()->PreAcceptCallback(1234, 0, replies);

    preaccept_ok_msg.release_dep();
    reply.release_preaccept_ok();

    // verify the correct metadata and behavior when a shardclient invokes the client's callback
    janusstore::Client::PendingRequest* req = Client()->pendingReqs.at(1234);
    EXPECT_EQ(req->responded_shards.size(),1);
    EXPECT_EQ(req->responded_shards.find(0) != req->responded_shards.end(),true);
    EXPECT_EQ(req->has_fast_quorum,true);
    EXPECT_EQ(req->aggregated_deps.size(),1);
    EXPECT_EQ(req->aggregated_deps.find(4567) != req->aggregated_deps.end(),true);
}
