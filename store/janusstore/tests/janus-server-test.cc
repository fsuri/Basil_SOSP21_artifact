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
    vector<ReplicaAddress> replicaAddrs;
    std::unique_ptr<transport::Configuration> config;
    SimulatedTransport *transport;

    // TODO what is this for?
    map<int, std::vector<ReplicaAddress>> *g_replicas;
    
    janusstore::Server *server;
    // std::unique_ptr<janusstore::Client> client;
    std::vector<janusstore::Server*> replicas;

    int shards;
    int replicas_per_shard;

    JanusServerTest() : shards(1), replicas_per_shard(3) {
        replicaAddrs = { 
            { "localhost", "12345" },
            { "localhost", "12346" },
            { "localhost", "12347" }
        };

        config = std::unique_ptr<transport::Configuration>(
            new transport::Configuration(
                replicas_per_shard, shards, replicaAddrs));

        g_replicas = new std::map<int, std::vector<ReplicaAddress>>({{ 0, replicaAddrs }});

        transport = new SimulatedTransport();

        for (int i = 0; i < replicas_per_shard; i++) {
            auto p = new janusstore::Server(*config, 0, i, transport);
            replicas.push_back(std::move(p));
        }

        server = replicas.front();
        // client = std::unique_ptr<janusstore::Client>(new janusstore::Client());
    };

    virtual int ServerGroup() {
        return server->groupIdx;
    }

    virtual int ServerId() {
        return server->myIdx;
    }
};

TEST_F(JanusServerTest, Init)
{
    EXPECT_EQ(ServerGroup(), 0);
    EXPECT_EQ(ServerId(), 0);
}
