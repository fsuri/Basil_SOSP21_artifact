#include "indicus_interface.h"


#include <iostream>
#include <cstring>
#include <cassert>
#include <algorithm>
#include <random>
#include <unistd.h>
#include <signal.h>

#include "salticidae/stream.h"
#include "salticidae/util.h"
#include "salticidae/network.h"
#include "salticidae/msg.h"

#include "hotstuff/promise.hpp"
#include "hotstuff/type.h"
#include "hotstuff/entity.h"
#include "hotstuff/util.h"
#include "hotstuff/client.h"
#include "hotstuff/hotstuff.h"
#include "hotstuff/liveness.h"

#include <string>
using std::string;

using salticidae::MsgNetwork;
using salticidae::ClientNetwork;
using salticidae::ElapsedTime;
using salticidae::Config;
using salticidae::_1;
using salticidae::_2;
using salticidae::static_pointer_cast;
using salticidae::trim_all;
using salticidae::split;

using hotstuff::TimerEvent;
using hotstuff::EventContext;
using hotstuff::NetAddr;
using hotstuff::HotStuffError;
using hotstuff::CommandDummy;
using hotstuff::Finality;
using hotstuff::command_t;
using hotstuff::uint256_t;
using hotstuff::opcode_t;
using hotstuff::bytearray_t;
using hotstuff::DataStream;
using hotstuff::ReplicaID;
using hotstuff::MsgReqCmd;
using hotstuff::MsgRespCmd;
using hotstuff::get_hash;
using hotstuff::promise_t;

using HotStuff = hotstuff::HotStuffSecp256k1;


namespace hotstuffstore {
    void IndicusInterface::propose(const std::string& hash, hotstuff_exec_callback execb) {
        //std::cout << "############# HotStuff Interface #############" << std::endl;
        execb(hash);
    }

    IndicusInterface::IndicusInterface(int shardId, int replicaId):
        shardId(shardId), replicaId(replicaId)
    {
        string config_dir = config_dir_base + "shard" + std::to_string(shardId) + "/";

        string config_file = config_dir + "hotstuff.gen.conf";
        string key_file = config_dir + "hotstuff.gen-sec" + std::to_string(replicaId) + ".conf";
        
        char* argv[4];
        char arg1[200];
        char arg3[200];
        memcpy(arg1, config_file.c_str(), config_file.length());
        memcpy(arg3, key_file.c_str(), key_file.length());
        argv[0] = "command";
        argv[1] = arg1;
        argv[2] = "--conf";
        argv[3] = arg3;

        std::cout << std::endl << "############## HotStuff Config: " << config_file << "   " << key_file << std::endl << std::endl;

        initialize(4, argv);
    }

    void IndicusInterface::initialize(int argc, char** argv) {
        Config config(argv[1]);
        //Config config("hotstuff.conf");

        ElapsedTime elapsed;
        elapsed.start();

        auto opt_blk_size = Config::OptValInt::create(1);
        auto opt_parent_limit = Config::OptValInt::create(-1);
        auto opt_stat_period = Config::OptValDouble::create(10);
        auto opt_replicas = Config::OptValStrVec::create();
        auto opt_idx = Config::OptValInt::create(0);
        auto opt_client_port = Config::OptValInt::create(-1);
        auto opt_privkey = Config::OptValStr::create();
        auto opt_tls_privkey = Config::OptValStr::create();
        auto opt_tls_cert = Config::OptValStr::create();
        auto opt_help = Config::OptValFlag::create(false);
        auto opt_pace_maker = Config::OptValStr::create("dummy");
        auto opt_fixed_proposer = Config::OptValInt::create(1);
        auto opt_base_timeout = Config::OptValDouble::create(1);
        auto opt_prop_delay = Config::OptValDouble::create(1);
        auto opt_imp_timeout = Config::OptValDouble::create(11);
        auto opt_nworker = Config::OptValInt::create(1);
        auto opt_repnworker = Config::OptValInt::create(1);
        auto opt_repburst = Config::OptValInt::create(1000000);
        auto opt_clinworker = Config::OptValInt::create(8);
        auto opt_cliburst = Config::OptValInt::create(1000000);
        auto opt_notls = Config::OptValFlag::create(false);
        auto opt_max_rep_msg = Config::OptValInt::create(4 << 20); // 4M by default
        auto opt_max_cli_msg = Config::OptValInt::create(65536); // 64K by default

        config.add_opt("block-size", opt_blk_size, Config::SET_VAL);
        config.add_opt("parent-limit", opt_parent_limit, Config::SET_VAL);
        config.add_opt("stat-period", opt_stat_period, Config::SET_VAL);
        config.add_opt("replica", opt_replicas, Config::APPEND, 'a', "add an replica to the list");
        config.add_opt("idx", opt_idx, Config::SET_VAL, 'i', "specify the index in the replica list");
        config.add_opt("cport", opt_client_port, Config::SET_VAL, 'c', "specify the port listening for clients");
        config.add_opt("privkey", opt_privkey, Config::SET_VAL);
        config.add_opt("tls-privkey", opt_tls_privkey, Config::SET_VAL);
        config.add_opt("tls-cert", opt_tls_cert, Config::SET_VAL);
        config.add_opt("pace-maker", opt_pace_maker, Config::SET_VAL, 'p', "specify pace maker (dummy, rr)");
        config.add_opt("proposer", opt_fixed_proposer, Config::SET_VAL, 'l', "set the fixed proposer (for dummy)");
        config.add_opt("base-timeout", opt_base_timeout, Config::SET_VAL, 't', "set the initial timeout for the Round-Robin Pacemaker");
        config.add_opt("prop-delay", opt_prop_delay, Config::SET_VAL, 't', "set the delay that follows the timeout for the Round-Robin Pacemaker");
        config.add_opt("imp-timeout", opt_imp_timeout, Config::SET_VAL, 'u', "set impeachment timeout (for sticky)");
        config.add_opt("nworker", opt_nworker, Config::SET_VAL, 'n', "the number of threads for verification");
        config.add_opt("repnworker", opt_repnworker, Config::SET_VAL, 'm', "the number of threads for replica network");
        config.add_opt("repburst", opt_repburst, Config::SET_VAL, 'b', "");
        config.add_opt("clinworker", opt_clinworker, Config::SET_VAL, 'M', "the number of threads for client network");
        config.add_opt("cliburst", opt_cliburst, Config::SET_VAL, 'B', "");
        config.add_opt("notls", opt_notls, Config::SWITCH_ON, 's', "disable TLS");
        config.add_opt("max-rep-msg", opt_max_rep_msg, Config::SET_VAL, 'S', "the maximum replica message size");
        config.add_opt("max-cli-msg", opt_max_cli_msg, Config::SET_VAL, 'S', "the maximum client message size");
        config.add_opt("help", opt_help, Config::SWITCH_ON, 'h', "show this help info");
        
        // EventContext ec;
        config.parse(argc, argv);
        // if (opt_help->get())
        //     {
        //         config.print_help();
        //         exit(0);
        //     }

        std::cout << "################### HotStuff batch size config: " << opt_blk_size->get() << std::endl;
    }
}

