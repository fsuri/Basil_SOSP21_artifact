#include "store/janusstore/transaction.h"
#include "store/janusstore/server.h"
#include "store/janusstore/client.h"
#include "store/janusstore/janus-proto.pb.h"

#include "store/common/stats.h"

#include "lib/simtransport.h"

#include <gtest/gtest.h>

#include <stdio.h>

using namespace transport;
using std::vector;
using std::map;

class TestReceiver : public TransportReceiver
{
public:
    TestReceiver() {}
    void ReceiveMessage(const TransportAddress &src,
                        const string &type, const string &data, void * meta_data) {}
};

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

    TestReceiver *receiver0;

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

        receiver0 = new TestReceiver();
        transport->Register(receiver0, *config, 1);
    };

    virtual void TearDown() {
        delete server;
        delete transport;
    }

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

/*************************************************************************
    BuildDepList
*************************************************************************/


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

/*************************************************************************
    _StronglyConnectedComponent
*************************************************************************/


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

/*************************************************************************
    _ReadyToProcess
*************************************************************************/

TEST_F(JanusServerTest, ReadyToProcessNoDeps)
{
    std::unordered_map<uint64_t, std::vector<uint64_t>> sample_dep_map {
        { 1234, {  }},
    };
    server->dep_map = sample_dep_map;

    janusstore::Transaction txn1(1234);
    janusstore::Transaction* txn_ptr1 = &txn1;
    txn_ptr1->setTransactionStatus(janusstore::proto::TransactionMessage::COMMIT);
    server->processed[1234] = false;
    EXPECT_TRUE(server->_ReadyToProcess(txn1));
}

TEST_F(JanusServerTest, ReadyToProcessWithDeps)
{
    std::unordered_map<uint64_t, std::vector<uint64_t>> sample_dep_map {
        { 1234, { 4567 }},
        { 4567, { 7890 }},
        { 7890, { 1234 }}
    };
    server->dep_map = sample_dep_map;

    janusstore::Transaction txn1(1234);
    janusstore::Transaction* txn_ptr1 = &txn1;
    txn_ptr1->setTransactionStatus(janusstore::proto::TransactionMessage::COMMIT);
    server->processed[4567] = true;
    server->processed[7890] = true;
    EXPECT_TRUE(server->_ReadyToProcess(txn1));
}

TEST_F(JanusServerTest, ReadyToProcessNotCommitting)
{
    janusstore::Transaction txn1(1234);
    janusstore::Transaction* txn_ptr1 = &txn1;
    txn_ptr1->setTransactionStatus(janusstore::proto::TransactionMessage::PREACCEPT);
    EXPECT_FALSE(server->_ReadyToProcess(txn1));
}

TEST_F(JanusServerTest, ReadyToProcessAlreadyProcessed)
{
    janusstore::Transaction txn1(1234);
    janusstore::Transaction* txn_ptr1 = &txn1;
    txn_ptr1->setTransactionStatus(janusstore::proto::TransactionMessage::COMMIT);
    server->processed[1234] = true;
    EXPECT_FALSE(server->_ReadyToProcess(txn1));
}

TEST_F(JanusServerTest, ReadyToProcessDepNotReady)
{
    std::unordered_map<uint64_t, std::vector<uint64_t>> sample_dep_map {
        { 1234, { 4567 }},
        { 4567, {  }}
    };
    server->dep_map = sample_dep_map;
    janusstore::Transaction txn1(1234);
    janusstore::Transaction* txn_ptr1 = &txn1;
    txn_ptr1->setTransactionStatus(janusstore::proto::TransactionMessage::COMMIT);
    server->processed[1234] = false;
    server->processed[4567] = false;
    EXPECT_FALSE(server->_ReadyToProcess(txn1));
}

/*************************************************************************
    HandlePreAccept
*************************************************************************/

TEST_F(JanusServerTest, HandlePreAcceptWorks)
{
    janusstore::Transaction txn1(1234);
    janusstore::Transaction* txn_ptr1 = &txn1;
    txn_ptr1->setTransactionStatus(janusstore::proto::TransactionMessage::PREACCEPT);
    txn_ptr1->addReadSet("key1");
    txn_ptr1->addWriteSet("key2", "val2");

    janusstore::proto::PreAcceptMessage pa_msg;
    janusstore::proto::TransactionMessage txn_msg;
    replication::ir::proto::UnloggedReplyMessage unlogged_reply;
    txn_ptr1->serialize(&txn_msg, 0);

    pa_msg.set_ballot(0);
    pa_msg.set_allocated_txn(&txn_msg);

    SimulatedTransportAddress *addr = new SimulatedTransportAddress(1);

    server->HandlePreAccept(*addr, pa_msg, &unlogged_reply);

    // check values of the reply
    janusstore::proto::Reply reply;
    janusstore::proto::PreAcceptOKMessage preaccept_ok_msg;

    reply.ParseFromString(unlogged_reply.reply());

    preaccept_ok_msg = reply.preaccept_ok();

    EXPECT_EQ(preaccept_ok_msg.txnid(), 1234);
    EXPECT_EQ(preaccept_ok_msg.dep().txnid_size(), 0);

    pa_msg.release_txn();
}

TEST_F(JanusServerTest, HandlePreAcceptSendsNotOK)
{
    janusstore::Transaction txn1(1234);
    janusstore::Transaction* txn_ptr1 = &txn1;
    txn_ptr1->setTransactionStatus(janusstore::proto::TransactionMessage::PREACCEPT);
    txn_ptr1->addReadSet("key1");
    txn_ptr1->addWriteSet("key2", "val2");

    janusstore::proto::PreAcceptMessage pa_msg;
    janusstore::proto::TransactionMessage txn_msg;
    replication::ir::proto::UnloggedReplyMessage unlogged_reply;
    txn_ptr1->serialize(&txn_msg, 0);

    pa_msg.set_ballot(0);
    pa_msg.set_allocated_txn(&txn_msg);

    SimulatedTransportAddress *addr = new SimulatedTransportAddress(1);

    server->HandlePreAccept(*addr, pa_msg, &unlogged_reply);

    // now send same txn id with higher ballot, this should get rejected
    pa_msg.set_ballot(1);
    server->HandlePreAccept(*addr, pa_msg, &unlogged_reply);

    // check values of the reply
    janusstore::proto::Reply reply;
    janusstore::proto::PreAcceptNotOKMessage preaccept_not_ok_msg;

    reply.ParseFromString(unlogged_reply.reply());
    preaccept_not_ok_msg = reply.preaccept_not_ok();

    EXPECT_EQ(preaccept_not_ok_msg.txnid(), 1234);

    pa_msg.release_txn();
}

/*************************************************************************
    HandleAccept
*************************************************************************/
TEST_F(JanusServerTest, HandleAcceptSendsOK)
{
    janusstore::Transaction txn1(1234);
    janusstore::Transaction* txn_ptr1 = &txn1;
    txn_ptr1->setTransactionStatus(janusstore::proto::TransactionMessage::PREACCEPT);
    txn_ptr1->addReadSet("key1");
    txn_ptr1->addWriteSet("key2", "val2");

    janusstore::Transaction txn2(1235);
    janusstore::Transaction* txn_ptr2 = &txn2;
    txn_ptr2->addReadSet("key1");
    txn_ptr2->addWriteSet("key2", "val3");

    server->id_txn_map[1234] = txn1;
    server->id_txn_map[1235] = txn2;

    server->BuildDepList(txn1, 0);
    vector<uint64_t> *dep2 = server->BuildDepList(txn2, 0);

    janusstore::proto::DependencyList dep;
    for (auto i : *dep2) {
        dep.add_txnid(i);
    }
    dep.add_txnid(1236);

    janusstore::proto::AcceptMessage a_msg;
    a_msg.set_txnid(1235);
    a_msg.set_ballot(2);
    a_msg.set_allocated_dep(&dep);

    SimulatedTransportAddress *addr = new SimulatedTransportAddress(1);
    replication::ir::proto::UnloggedReplyMessage unlogged_reply;
    server->HandleAccept(*addr, a_msg, &unlogged_reply);

    janusstore::proto::Reply reply;
    janusstore::proto::AcceptOKMessage a_ok_msg;

    reply.ParseFromString(unlogged_reply.reply());
    a_ok_msg = reply.accept_ok();

    EXPECT_EQ(a_ok_msg.txnid(), 1235);
    EXPECT_EQ(server->id_txn_map[1235].getTransactionStatus(), janusstore::proto::TransactionMessage::ACCEPT);
    EXPECT_EQ(server->accepted_ballots[1235], 2);
    EXPECT_EQ(
        server->id_txn_map[1235].getTransactionStatus(),
        janusstore::proto::TransactionMessage::ACCEPT
    );
    //check to see if dep_map updated with 1236
    vector<uint64_t> server_dep_map = server->dep_map[1235];
    EXPECT_TRUE(
        std::find(
            server_dep_map.begin(),
            server_dep_map.end(),
            1236
        ) != server_dep_map.end()
    );

    a_msg.release_dep();
}

TEST_F(JanusServerTest, HandleAcceptSendsNotOKBallot)
{
    janusstore::Transaction txn1(1234);
    janusstore::Transaction* txn_ptr1 = &txn1;
    txn_ptr1->setTransactionStatus(janusstore::proto::TransactionMessage::PREACCEPT);

    server->id_txn_map[1234] = txn1;
    vector<uint64_t> *dep1 = server->BuildDepList(txn1, 0);
    server->accepted_ballots[1234] = 2;

    janusstore::proto::DependencyList dep;
    for (auto i : *dep1) {
        dep.add_txnid(i);
    }

    janusstore::proto::AcceptMessage a_msg;
    a_msg.set_txnid(1234);
    a_msg.set_ballot(1); // 1 < 2 so this should be rejected
    a_msg.set_allocated_dep(&dep);

    SimulatedTransportAddress *addr = new SimulatedTransportAddress(1);
    replication::ir::proto::UnloggedReplyMessage unlogged_reply;
    server->HandleAccept(*addr, a_msg, &unlogged_reply);

    janusstore::proto::Reply reply;
    janusstore::proto::AcceptNotOKMessage a_not_ok_msg;

    reply.ParseFromString(unlogged_reply.reply());
    a_not_ok_msg = reply.accept_not_ok();

    EXPECT_EQ(a_not_ok_msg.txnid(), 1234);
    EXPECT_EQ(a_not_ok_msg.highest_ballot(), 2);

    a_msg.release_dep();
}

TEST_F(JanusServerTest, HandleAcceptSendsNotCommit)
{
    janusstore::Transaction txn1(1234);
    janusstore::Transaction* txn_ptr1 = &txn1;
    vector<uint64_t> *dep1 = server->BuildDepList(*txn_ptr1, 0);
    txn_ptr1->setTransactionStatus(janusstore::proto::TransactionMessage::COMMIT);
    server->id_txn_map[1234] = *txn_ptr1;

    janusstore::proto::DependencyList dep;
    for (auto i : *dep1) {
        dep.add_txnid(i);
    }

    janusstore::proto::AcceptMessage a_msg;
    a_msg.set_txnid(1234);
    a_msg.set_ballot(1); // 1 < 2 so this should be rejected
    a_msg.set_allocated_dep(&dep);

    SimulatedTransportAddress *addr = new SimulatedTransportAddress(1);
    replication::ir::proto::UnloggedReplyMessage unlogged_reply;
    server->HandleAccept(*addr, a_msg, &unlogged_reply);

    janusstore::proto::Reply reply;
    janusstore::proto::AcceptNotOKMessage a_not_ok_msg;

    reply.ParseFromString(unlogged_reply.reply());
    a_not_ok_msg = reply.accept_not_ok();

    EXPECT_EQ(a_not_ok_msg.txnid(), 1234);
    EXPECT_EQ(a_not_ok_msg.highest_ballot(), 0);

    a_msg.release_dep();
}

/*************************************************************************
    HandleInquireReply
*************************************************************************/

TEST_F(JanusServerTest, HandleInquireReplyUpdates)
{
    janusstore::Transaction txn1(1234);
    janusstore::Transaction* txn_ptr1 = &txn1;
    vector<uint64_t> *dep1 = server->BuildDepList(*txn_ptr1, 0);
    txn_ptr1->setTransactionStatus(janusstore::proto::TransactionMessage::PREACCEPT);
    server->id_txn_map[1234] = *txn_ptr1;

    janusstore::proto::InquireOKMessage i_ok_msg;
    janusstore::proto::DependencyList dep;
    for (auto i : *dep1) {
        dep.add_txnid(i);
    }
    dep.add_txnid(1235);

    i_ok_msg.set_txnid(1234);
    i_ok_msg.set_allocated_dep(&dep);

    server->HandleInquireReply(i_ok_msg);

    EXPECT_EQ(
        server->id_txn_map[1234].getTransactionStatus(),
        janusstore::proto::TransactionMessage::COMMIT
    );

    EXPECT_EQ(
        server->dep_map[1234].size(),
        1
    );

    i_ok_msg.release_dep();
}

TEST_F(JanusServerTest, HandleInquireReplyIgnores)
{
    janusstore::Transaction txn1(1234);
    janusstore::Transaction* txn_ptr1 = &txn1;
    vector<uint64_t> *dep1 = server->BuildDepList(*txn_ptr1, 0);
    txn_ptr1->setTransactionStatus(janusstore::proto::TransactionMessage::COMMIT);
    server->id_txn_map[1234] = *txn_ptr1;

    janusstore::proto::InquireOKMessage i_ok_msg;
    janusstore::proto::DependencyList dep;
    for (auto i : *dep1) {
        dep.add_txnid(i);
    }
    dep.add_txnid(1235);

    i_ok_msg.set_txnid(1234);
    i_ok_msg.set_allocated_dep(&dep);

    server->HandleInquireReply(i_ok_msg);

    EXPECT_EQ(
        server->id_txn_map[1234].getTransactionStatus(),
        janusstore::proto::TransactionMessage::COMMIT
    );

    EXPECT_EQ(
        server->dep_map[1234].size(),
        0
    );

    i_ok_msg.release_dep();
}

/*************************************************************************
    _HandleInquire
*************************************************************************/
TEST_F(JanusServerTest, HandleInquireWhenCommitting)
{

    janusstore::Transaction txn1(1234);
    janusstore::Transaction* txn_ptr1 = &txn1;
    txn_ptr1->addReadSet("key1");
    txn_ptr1->addWriteSet("key2", "val2");

    janusstore::Transaction txn2(1235);
    janusstore::Transaction* txn_ptr2 = &txn2;
    txn_ptr2->addReadSet("key1");
    txn_ptr2->addWriteSet("key2", "val3");

    server->id_txn_map[1234] = *txn_ptr1;
    server->id_txn_map[1235] = *txn_ptr2;

    server->BuildDepList(txn2, 0);
    server->BuildDepList(txn1, 0); // so txn1 has 1235 as a dep
    server->id_txn_map[1234].setTransactionStatus(janusstore::proto::TransactionMessage::COMMIT);

    janusstore::proto::Reply reply;
    janusstore::proto::InquireMessage i_msg;
    i_msg.set_txnid(1234);

    SimulatedTransportAddress *addr = new SimulatedTransportAddress(1);
    server->HandleInquire(*addr, i_msg, &reply);

    EXPECT_EQ(reply.op(), janusstore::proto::Reply::INQUIRE_OK);

    janusstore::proto::InquireOKMessage i_ok_msg = reply.inquire_ok();
    EXPECT_EQ(i_ok_msg.txnid(), 1234);
    EXPECT_EQ(i_ok_msg.dep().txnid_size(), 1);
}

TEST_F(JanusServerTest, HandleInquireBlockWhenNotCommit)
{

    janusstore::Transaction txn1(1234);
    janusstore::Transaction* txn_ptr1 = &txn1;
    txn_ptr1->addReadSet("key1");
    txn_ptr1->addWriteSet("key2", "val2");

    janusstore::Transaction txn2(1235);
    janusstore::Transaction* txn_ptr2 = &txn2;
    txn_ptr2->addReadSet("key1");
    txn_ptr2->addWriteSet("key2", "val3");

    server->id_txn_map[1234] = *txn_ptr1;
    server->id_txn_map[1235] = *txn_ptr2;

    server->BuildDepList(txn2, 0);
    server->BuildDepList(txn1, 0); // so txn1 has 1235 as a dep

    janusstore::proto::Reply reply;
    janusstore::proto::InquireMessage i_msg;
    i_msg.set_txnid(1234);

    SimulatedTransportAddress *addr = new SimulatedTransportAddress(1);
    server->HandleInquire(*addr, i_msg, &reply);

    EXPECT_EQ(server->inquired_ids[1234].size(), 1);
    EXPECT_EQ(server->inquired_ids[1234][0].first, addr);
    EXPECT_EQ(server->inquired_ids[1234][0].second.txnid(), i_msg.txnid());
}

/*************************************************************************
    _HandleCommit
*************************************************************************/
TEST_F(JanusServerTest, HandleCommitExecutes)
{
    janusstore::Transaction txn1(1234);
    janusstore::Transaction* txn_ptr1 = &txn1;
    txn_ptr1->addWriteSet("key2", "val2");
    server->BuildDepList(txn1, 0);

    replication::ir::proto::UnloggedReplyMessage unlogged_reply;
    SimulatedTransportAddress *addr = new SimulatedTransportAddress(1);

    server->_HandleCommit(1234, *addr, &unlogged_reply);

    janusstore::proto::Reply reply;
    reply.ParseFromString(unlogged_reply.reply());

    EXPECT_EQ(reply.op(), janusstore::proto::Reply::COMMIT_OK);
    janusstore::proto::CommitOKMessage commit_ok = reply.commit_ok();

    EXPECT_EQ(commit_ok.pairs(0).key(), "key2");
    EXPECT_EQ(commit_ok.pairs(0).value(), "val2");
}

TEST_F(JanusServerTest, HandleCommitInquireIds)
{

}

TEST_F(JanusServerTest, HandleCommitUnblockIds)
{

}

TEST_F(JanusServerTest, HandleCommitBlockedIds)
{
    janusstore::Transaction txn1(1234);
    janusstore::Transaction* txn_ptr1 = &txn1;
    txn_ptr1->addWriteSet("key2", "val2");

    janusstore::Transaction txn2(1235);
    janusstore::Transaction* txn_ptr2 = &txn2;
    txn_ptr2->addWriteSet("key2", "val3");

    janusstore::Transaction txn3(1111);
    janusstore::Transaction* txn_ptr3 = &txn3;
    txn_ptr3->addWriteSet("key2", "val4");

    server->id_txn_map[1234] = *txn_ptr1;
    server->id_txn_map[1235] = *txn_ptr2;
    server->id_txn_map[1111] = *txn_ptr3;

    server->BuildDepList(txn2, 0);
    server->BuildDepList(txn1, 0); // so txn1 has 1235 as a dep
    server->BuildDepList(txn3, 0); // so txn3 has 1235 as a dep

    replication::ir::proto::UnloggedReplyMessage unlogged_reply;
    SimulatedTransportAddress *addr = new SimulatedTransportAddress(1);

    server->_HandleCommit(1111, *addr, &unlogged_reply);
    EXPECT_EQ(server->blocking_ids.size(), 2);
    EXPECT_EQ(server->blocking_ids[1235].size(), 1);
    EXPECT_TRUE(server->blocking_ids[1235].find(1111) != server->blocking_ids[1235].end());

    server->_HandleCommit(1234, *addr, &unlogged_reply);
    EXPECT_EQ(server->blocking_ids[1235].size(), 2);
    EXPECT_TRUE(server->blocking_ids[1235].find(1234) != server->blocking_ids[1235].end());

}
/*************************************************************************
    _ExecutePhase
*************************************************************************/
TEST_F(JanusServerTest, ExecutePhaseExecutesSingleTxn)
{

}

TEST_F(JanusServerTest, ExecutePhaseExecutesMultTxn)
{

}

TEST_F(JanusServerTest, ExecutePhaseExecutesCircularDep)
{

}
