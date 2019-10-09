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

    EXPECT_EQ(server->groupIdx, 0);
    EXPECT_EQ(server->myIdx, 0);
}
