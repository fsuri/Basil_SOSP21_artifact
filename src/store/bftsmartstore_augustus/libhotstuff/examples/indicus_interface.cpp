#include "indicus_interface.h"
#include "hotstuff_app.cpp"

namespace hotstuff {
  int hotstuff_core_offset;
}

namespace bftsmartstore_augustus {

    void IndicusInterface::propose(const std::string& hash, hotstuff_exec_callback execb) {
        //std::cout << "############# HotStuff Interface #############" << std::endl;
        hotstuff_papp->interface_propose(hash, execb);
    }

    IndicusInterface::IndicusInterface(int shardId, int replicaId, int cpuId):
        shardId(shardId), replicaId(replicaId), cpuId(cpuId)
    {

        hotstuff::hotstuff_core_offset = (cpuId + 4) % 8;

        string config_dir = config_dir_base + "shard" + std::to_string(shardId) + "/";

        string config_file = config_dir + "hotstuff.gen.conf";
        string key_file = config_dir + "hotstuff.gen-sec" + std::to_string(replicaId) + ".conf";

        char* argv[4];
        char arg1[200];
        char arg3[200];
        memset(arg1, 0, sizeof(arg1));
        memset(arg3, 0, sizeof(arg3));
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
        auto opt_base_timeout = Config::OptValDouble::create(10000);
        auto opt_prop_delay = Config::OptValDouble::create(1);
        auto opt_imp_timeout = Config::OptValDouble::create(10000);
        auto opt_nworker = Config::OptValInt::create(3);
        auto opt_repnworker = Config::OptValInt::create(3);
        auto opt_repburst = Config::OptValInt::create(1000000);
        auto opt_clinworker = Config::OptValInt::create(1);
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

        EventContext ec;
        config.parse(argc, argv);
        if (opt_help->get())
            {
                config.print_help();
                exit(0);
            }
        auto idx = opt_idx->get();
        auto client_port = opt_client_port->get();
        std::vector<std::tuple<std::string, std::string, std::string>> replicas;
        for (const auto &s: opt_replicas->get())
            {
                auto res = trim_all(split(s, ","));
                if (res.size() != 3)
                    throw HotStuffError("invalid replica info");
                replicas.push_back(std::make_tuple(res[0], res[1], res[2]));
            }

        if (!(0 <= idx && (size_t)idx < replicas.size())) {
            std::cout << "########## OUT OF RANGE INDEX: " << idx << " < " << replicas.size() << std::endl;
            throw HotStuffError("replica idx out of range");
        }
        std::string binding_addr = std::get<0>(replicas[idx]);
        if (client_port == -1)
            {
                auto p = split_ip_port_cport(binding_addr);
                size_t idx;
                try {
                    client_port = stoi(p.second, &idx);
                } catch (std::invalid_argument &) {
                    throw HotStuffError("client port not specified");
                }
            }

        NetAddr plisten_addr{split_ip_port_cport(binding_addr).first};

        auto parent_limit = opt_parent_limit->get();
        hotstuff::pacemaker_bt pmaker;
        if (opt_pace_maker->get() == "dummy")
            pmaker = new hotstuff::PaceMakerDummyFixed(opt_fixed_proposer->get(), parent_limit);
        else
            pmaker = new hotstuff::PaceMakerRR(ec, parent_limit, opt_base_timeout->get(), opt_prop_delay->get());

        HotStuffApp::Net::Config repnet_config;
        ClientNetwork<opcode_t>::Config clinet_config;
        repnet_config.max_msg_size(opt_max_rep_msg->get());
        clinet_config.max_msg_size(opt_max_cli_msg->get());
        if (!opt_tls_privkey->get().empty() && !opt_notls->get())
            {
                auto tls_priv_key = new salticidae::PKey(
                                                         salticidae::PKey::create_privkey_from_der(
                                                                                                   hotstuff::from_hex(opt_tls_privkey->get())));
                auto tls_cert = new salticidae::X509(
                                                     salticidae::X509::create_from_der(
                                                                                       hotstuff::from_hex(opt_tls_cert->get())));
                repnet_config
                    .enable_tls(true)
                    .tls_key(tls_priv_key)
                    .tls_cert(tls_cert);
            }
        repnet_config
            .burst_size(opt_repburst->get())
            .nworker(opt_repnworker->get());
        clinet_config
            .burst_size(opt_cliburst->get())
            .nworker(opt_clinworker->get());
        hotstuff_papp = new HotStuffApp(opt_blk_size->get(),
                               opt_stat_period->get(),
                               opt_imp_timeout->get(),
                               idx,
                               hotstuff::from_hex(opt_privkey->get()),
                               plisten_addr,
                               NetAddr("0.0.0.0", client_port),
                               std::move(pmaker),
                               ec,
                               opt_nworker->get(),
                               repnet_config,
                               clinet_config);
        std::vector<std::tuple<NetAddr, bytearray_t, bytearray_t>> reps;
        for (auto &r: replicas)
            {
                auto p = split_ip_port_cport(std::get<0>(r));
                reps.push_back(std::make_tuple(
                                               NetAddr(p.first),
                                               hotstuff::from_hex(std::get<1>(r)),
                                               hotstuff::from_hex(std::get<2>(r))));
            }
        auto shutdown = [&](int) { hotstuff_papp->stop(); };
        salticidae::SigEvent ev_sigint(ec, shutdown);
        salticidae::SigEvent ev_sigterm(ec, shutdown);
        ev_sigint.add(SIGINT);
        ev_sigterm.add(SIGTERM);

        hotstuff_papp->start(reps);

        // spawning a new thread to run hotstuff logic asynchronously
        std::thread t([this](){
                cpu_set_t cpuset;
                CPU_ZERO(&cpuset);
                CPU_SET(cpuId, &cpuset);
                pthread_setaffinity_np(pthread_self(),	sizeof(cpu_set_t), &cpuset);
                std::cout << "HotStuff runs on CPU" << cpuId << std::endl;
                hotstuff_papp->interface_entry();
                //elapsed.stop(true);
            });
        t.detach();
    }
}
