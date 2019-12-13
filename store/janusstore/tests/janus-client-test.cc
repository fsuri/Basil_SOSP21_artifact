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
    map<int, std::vector<ReplicaAddress>> *g_replicas_multishard;

    transport::Configuration *config;
    transport::Configuration *config_multishard;
    SimulatedTransport *transport;
    SimulatedTransport *transport_multishard;

    janusstore::Client *client;
    janusstore::Client *client_multishard;
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

        vector<ReplicaAddress> replicaAddrs_shard0 = {
            { "localhost", "12345" },
            { "localhost", "12346" },
            { "localhost", "12347" }
        };
        vector<ReplicaAddress> replicaAddrs_shard1 = {
            { "localhost", "22345" },
            { "localhost", "22346" },
            { "localhost", "22347" }
        };
        g_replicas_multishard = new std::map<int, std::vector<ReplicaAddress>>({{ 0, replicaAddrs_shard0 },
             { 1, replicaAddrs_shard1 }});

        config = new transport::Configuration(
                shards, replicas_per_shard, 1, *g_replicas);

        config_multishard = new transport::Configuration(
                2, replicas_per_shard, 1, *g_replicas_multishard);

        txn = new janusstore::Transaction(1234);
        txn->addReadSet("key1");
        txn->addReadSet("key2");
        txn->addWriteSet("key3", "val3");
        txn->setTransactionStatus(janusstore::proto::TransactionMessage::PREACCEPT);

        ballot = 0;

        transport = new SimulatedTransport();
        transport_multishard = new SimulatedTransport();

        client = new janusstore::Client(config, shards, 0, transport);
        client_multishard = new janusstore::Client(config_multishard, 2, 0, transport_multishard);
    };

    virtual janusstore::Client* Client() {
        return client;
    }

    virtual janusstore::Client* ClientMultiShard() {
        return client_multishard;
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

TEST_F(JanusClientTest, PreAcceptState) {

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
    EXPECT_EQ(req->has_fast_quorum,true);
    EXPECT_EQ(req->aggregated_deps.size(),1);
    EXPECT_EQ(req->aggregated_deps.find(4567) != req->aggregated_deps.end(),true);
}

/* TODO: fix test and uncomment
TEST_F(JanusClientTest, SlowPathTaken) {
    * 
     * Verifies: when two shards reply with different dependencies,
     * the fast quorum flag is set to false.
     *
    auto ccb = [] (uint64_t committed, std::map<std::string, std::string> readValues) {
        printf("output commit from txn %d \r\n", committed);
    };

    PreAcceptOKMessage preaccept_ok_msg_shard1;
    DependencyList dep_shard1;
    dep_shard1.add_txnid(4567);
    preaccept_ok_msg_shard1.set_txnid(1234);
    preaccept_ok_msg_shard1.set_allocated_dep(&dep_shard1);

    PreAcceptOKMessage preaccept_ok_msg_shard2;
    DependencyList dep_shard2;
    dep_shard2.add_txnid(6789);
    preaccept_ok_msg_shard2.set_txnid(1234);
    preaccept_ok_msg_shard2.set_allocated_dep(&dep_shard2);

    Reply reply_shard1;
    reply_shard1.set_op(Reply::PREACCEPT_OK);
    reply_shard1.set_allocated_preaccept_ok(&preaccept_ok_msg_shard1);

    Reply reply_shard2;
    reply_shard2.set_op(Reply::PREACCEPT_OK);
    reply_shard2.set_allocated_preaccept_ok(&preaccept_ok_msg_shard2);

    std::vector<Reply> replies_shard1 = {reply_shard1};
    std::vector<Reply> replies_shard2 = {reply_shard2};

    ClientMultiShard()->PreAccept(Transaction(), Ballot(), ccb);
    ClientMultiShard()->PreAcceptCallback(1234, 0, replies_shard1);
    ClientMultiShard()->PreAcceptCallback(1234, 1, replies_shard2);

    preaccept_ok_msg_shard1.release_dep();
    reply_shard1.release_preaccept_ok();
    preaccept_ok_msg_shard2.release_dep();
    reply_shard2.release_preaccept_ok();

    // verify the correct metadata and behavior when a shardclient invokes the client's callback
    janusstore::Client::PendingRequest* req = ClientMultiShard()->pendingReqs.at(1234);
    EXPECT_EQ(req->has_fast_quorum,false);
    EXPECT_EQ(req->aggregated_deps.size(),2);
    EXPECT_EQ(req->aggregated_deps.find(4567) != req->aggregated_deps.end(),true);
    EXPECT_EQ(req->aggregated_deps.find(6789) != req->aggregated_deps.end(),true);
}
*/
