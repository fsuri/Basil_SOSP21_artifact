#include "store/janusstore/server.h"
#include "store/common/stats.h"
#include "lib/simtransport.h"

#include <gtest/gtest.h>

using namespace transport;
using std::vector;

class JanusServerTest : public  ::testing::Test
{

};

TEST(Server, Basic)
{
    vector<ReplicaAddress> replicas = { { "localhost", "12345" },
    { "localhost", "12346" },
    { "localhost", "12347" } };
    std::map<int, std::vector<ReplicaAddress>> g_replicas({
        { 0, replicas }
    });
    Configuration c(3, 1, replicas);
    SimulatedTransport *transport = new SimulatedTransport();

    janusstore::Server *server = new janusstore::Server(c, 0, 0, transport);

    // EXPECT_EQ(server->groupIdx, 0);
    // EXPECT_EQ(server->myIdx, 0);
}

// TEST(Configuration, Multicast)
// {
//     vector<ReplicaAddress> replicas = { { "localhost", "12345" },
//                                         { "localhost", "12346" },
//                                         { "localhost", "12347" } };
//     ReplicaAddress multicast = { "localhost", "12348" };
//     Configuration c(3, 1, replicas, &multicast);

//     EXPECT_EQ(c.n, 3);
//     EXPECT_EQ(c.f, 1);
//     EXPECT_EQ(c.replica(0).host, "localhost");
//     EXPECT_EQ(c.replica(1).host, "localhost");
//     EXPECT_EQ(c.replica(2).host, "localhost");
//     EXPECT_EQ(c.replica(0).port, "12345");
//     EXPECT_EQ(c.replica(1).port, "12346");
//     EXPECT_EQ(c.replica(2).port, "12347");
//     ASSERT_NE(nullptr, c.multicast());
//     EXPECT_EQ(c.multicast()->host, "localhost");
//     EXPECT_EQ(c.multicast()->port, "12348");
// }

// TEST(Configuration, Quorum)
// {
//     vector<ReplicaAddress> replicas = { { "localhost", "12345" },
//                                         { "localhost", "12346" },
//                                         { "localhost", "12347" } };
//     Configuration c(3, 1, replicas);

//     EXPECT_EQ(c.n, 3);
//     EXPECT_EQ(c.f, 1);
//     EXPECT_EQ(c.QuorumSize(), 2);
//     EXPECT_EQ(c.FastQuorumSize(), 3);

//     replicas.push_back(ReplicaAddress("localhost", "12348"));
//     replicas.push_back(ReplicaAddress("localhost", "12349"));

//     Configuration c2(5, 2, replicas);
//     EXPECT_EQ(c2.n, 5);
//     EXPECT_EQ(c2.f, 2);
//     EXPECT_EQ(c2.QuorumSize(), 3);
//     EXPECT_EQ(c2.FastQuorumSize(), 4);

//     Configuration c3(5, 1, replicas);
//     EXPECT_EQ(c3.n, 5);
//     EXPECT_EQ(c3.f, 1);
//     EXPECT_EQ(c3.QuorumSize(), 2);
//     EXPECT_EQ(c3.FastQuorumSize(), 3);
// }

// TEST(Configuration, Leader)
// {
//     vector<ReplicaAddress> replicas = { { "localhost", "12345" },
//                                         { "localhost", "12346" },
//                                         { "localhost", "12347" } };
//     Configuration c(3, 1, replicas);
//     EXPECT_EQ(c.GetLeaderIndex(0), 0);
//     EXPECT_EQ(c.GetLeaderIndex(1), 1);
//     EXPECT_EQ(c.GetLeaderIndex(2), 2);
//     EXPECT_EQ(c.GetLeaderIndex(3), 0);
//     EXPECT_EQ(c.GetLeaderIndex(4), 1);
//     EXPECT_EQ(c.GetLeaderIndex(5), 2);
// }

// TEST(Configuration, FromFile)
// {
//     std::ifstream stream("lib/tests/configuration-test-1.conf");
//     Configuration c(stream);

//     EXPECT_EQ(c.n, 3);
//     EXPECT_EQ(c.f, 1);
//     EXPECT_EQ(c.replica(0).host, "localhost");
//     EXPECT_EQ(c.replica(1).host, "localhost");
//     EXPECT_EQ(c.replica(2).host, "localhost");
//     EXPECT_EQ(c.multicast()->host, "localhost");
//     EXPECT_EQ(c.replica(0).port, "12345");
//     EXPECT_EQ(c.replica(1).port, "12346");
//     EXPECT_EQ(c.replica(2).port, "12347");
//     EXPECT_EQ(c.multicast()->port, "12348");
// }

// TEST(Configuration, AddressEquality)
// {
//     ReplicaAddress a1("localhost", "12345");
//     ReplicaAddress a2("localhost", "12345");
//     ReplicaAddress b("localhost", "12346");
//     ReplicaAddress c("otherhost", "12346");

//     EXPECT_EQ(a1, a2);
//     EXPECT_NE(a1, b);
//     EXPECT_NE(a1, c);

//     EXPECT_EQ(std::hash<ReplicaAddress>()(a1), std::hash<ReplicaAddress>()(a2));
// }

// TEST(Configuration, Equality)
// {
//     vector<ReplicaAddress> replicasA = { { "localhost", "12345" },
//                                          { "localhost", "12346" },
//                                          { "localhost", "12347" } };

//     vector<ReplicaAddress> replicasA2 = { { "localhost", "12345" },
//                                           { "localhost", "12346" },
//                                           { "localhost", "12347" } };
//     vector<ReplicaAddress> replicasB = { { "otherhost", "12345" },
//                                          { "localhost", "12346" },
//                                          { "localhost", "12347" } };
//     vector<ReplicaAddress> replicasC = { { "localhost", "12345" },
//                                          { "localhost", "12346" },
//                                          { "localhost", "12347" },
//                                          { "localhost", "12348" },
//                                          { "localhost", "12349" } };

//     Configuration a1(3, 1, replicasA);
//     Configuration a2(3, 1, replicasA);
//     Configuration a3(3, 1, replicasA2);
//     Configuration b(3, 1, replicasB);
//     Configuration c(5, 2, replicasC);

//     EXPECT_EQ(a1, a2);
//     EXPECT_EQ(a1, a3);
//     EXPECT_NE(a1, b);
//     EXPECT_NE(a1, c);

//     ReplicaAddress multicastA("multicast", "11111");
//     ReplicaAddress multicastB("multicast", "22222");
//     ASSERT_NE(multicastA, multicastB);

//     Configuration d(3, 1, replicasA, &multicastA);
//     Configuration e(3, 1, replicasA, &multicastB);

//     EXPECT_NE(a1, d);
//     EXPECT_NE(a1, e);

//     EXPECT_EQ(std::hash<Configuration>()(a1), std::hash<Configuration>()(a2));
//     EXPECT_EQ(std::hash<Configuration>()(a1), std::hash<Configuration>()(a3));
// }
