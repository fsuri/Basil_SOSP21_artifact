#include "store/janusstore/server.h"
#include "store/common/stats.h"
#include "lib/simtransport.h"

#include <gtest/gtest.h>

using namespace transport;
using std::vector;
using std::map;

class JanusServerTest : public  ::testing::Test
{
protected:
    vector<ReplicaAddress> replicas;
    std::unique_ptr<transport::Configuration> config;
    SimulatedTransport *transport;
    
    janusstore::Server *server;

    map<int, std::vector<ReplicaAddress>> *g_replicas;

    JanusServerTest() {
        replicas = { 
            { "localhost", "12345" },
            { "localhost", "12346" },
            { "localhost", "12347" }
        };

        config = std::unique_ptr<transport::Configuration>(
            new transport::Configuration(3, 1, replicas));

        g_replicas = new std::map<int, std::vector<ReplicaAddress>>({{ 0, replicas }});

        transport = new SimulatedTransport();
        server = new janusstore::Server(*config, 0, 0, transport);
    };

    virtual int ServerGroup() {
        return server->groupIdx;
    }

    virtual int ServerId() {
        return server->myIdx;
    }
};

TEST_F(JanusServerTest, Basic)
{
    EXPECT_EQ(ServerGroup(), 0);
    EXPECT_EQ(ServerId(), 0);
}
