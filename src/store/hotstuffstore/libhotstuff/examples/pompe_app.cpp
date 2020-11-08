/**
 * Copyright 2018 VMware
 * Copyright 2018 Ted Yin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <iostream>
#include <cstring>
#include <cassert>
#include <algorithm>
#include <random>
#include <unistd.h>
#include <signal.h>
#include <fstream>

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
using hotstuff::Ordering1Finality;
using hotstuff::Ordering2Finality;
using hotstuff::command_t;
using hotstuff::uint256_t;
using hotstuff::opcode_t;
using hotstuff::bytearray_t;
using hotstuff::DataStream;
using hotstuff::ReplicaID;
using hotstuff::MsgReqCmd;
using hotstuff::MsgOrdering1ReqCmd;
using hotstuff::MsgOrdering1RespCmd;
using hotstuff::MsgOrdering2ReqCmd;
using hotstuff::MsgOrdering2RespCmd;
using hotstuff::MsgConsensusReqCmd;
using hotstuff::MsgConsensusRespCmd;
using hotstuff::MsgConsensusRespClientCmd;
using hotstuff::MsgRespCmd;
using hotstuff::get_hash;
using hotstuff::promise_t;

using HotStuff = hotstuff::HotStuffSecp256k1;

class HotStuffApp: public HotStuff {
    /** for Archipelago */
    using LedgerNet = salticidae::MsgNetwork<opcode_t>;
    ReplicaID idx;
    uint64_t exec_last_batch_clock; // stable point
    uint32_t clnt_blk_size;
    bool consensus_ready;
    std::unordered_map<const uint256_t, int> batch_received;
    std::vector<NetAddr> ledger_replicas;
    std::unordered_map<ReplicaID, LedgerNet::conn_t> ledger_conns;

    
    double stat_period;
    double impeach_timeout;
    EventContext ec;
    EventContext req_ec;
    EventContext resp_ec;
    /** Network messaging between a replica and its client. */
    ClientNetwork<opcode_t> cn;
    /** Timer object to schedule a periodic printing of system statistics */
    TimerEvent ev_stat_timer;
    /** Timer object to monitor the progress for simple impeachment */
    TimerEvent impeach_timer;
    /** Timer object to start Archipelago consensus phase */
    TimerEvent consensus_timer;
    /** The listen address for client RPC */
    NetAddr clisten_addr;

    std::unordered_map<const uint256_t, promise_t> unconfirmed;

    using conn_t = ClientNetwork<opcode_t>::conn_t;
    using resp_queue_t = salticidae::MPSCQueueEventDriven<std::pair<Finality, NetAddr>>;
    using ordering1_resp_queue_t = salticidae::MPSCQueueEventDriven<std::pair<Ordering1Finality, NetAddr>>;
    using ordering2_resp_queue_t = salticidae::MPSCQueueEventDriven<std::pair<Ordering2Finality, NetAddr>>;
    using consensus_resp_queue_t = salticidae::MPSCQueueEventDriven<std::pair<uint256_t, NetAddr>>;


    /* for the dedicated thread sending responses to the clients */
    std::thread req_thread;
    std::thread resp_thread;
    resp_queue_t resp_queue;
    ordering1_resp_queue_t ordering1_queue;
    ordering2_resp_queue_t ordering2_queue;
    consensus_resp_queue_t consensus_queue;
    salticidae::BoxObj<salticidae::ThreadCall> resp_tcall;
    salticidae::BoxObj<salticidae::ThreadCall> req_tcall;

    void client_request_cmd_handler(MsgReqCmd &&, const conn_t &);
    void client_ordering1_request_cmd_handler(MsgOrdering1ReqCmd &&, const conn_t &);
    void client_ordering2_request_cmd_handler(MsgOrdering2ReqCmd &&, const conn_t &);
    void client_ordering_exec_reply_handler(MsgRespCmd &&, const conn_t &);
    

    static command_t parse_cmd(DataStream &s) {
        auto cmd = new CommandDummy();
        s >> *cmd;
        return cmd;
    }

    void reset_imp_timer() {
        impeach_timer.del();
        impeach_timer.add(impeach_timeout);
    }

    void state_machine_execute(const Finality &fin) override {
        reset_imp_timer();
#ifndef HOTSTUFF_ENABLE_BENCHMARK
        HOTSTUFF_LOG_INFO("replicated %s", std::string(fin).c_str());
#endif
    }

    std::pair<std::string, std::string> split_ip_port_cport(const std::string &s) {
        auto ret = salticidae::trim_all(salticidae::split(s, ";"));
        return std::make_pair(ret[0], ret[1]);
    }
    

    //#ifdef HOTSTUFF_MSG_STAT
    std::unordered_set<conn_t> client_conns;
    void print_stat() const;
    //#endif

    public:
    HotStuffApp(uint32_t blk_size,
                uint32_t clnt_blk_size,
                uint32_t stable_period,
                uint32_t liveness_delta,
                double stat_period,
                double impeach_timeout,
                ReplicaID idx,
                const bytearray_t &raw_privkey,
                NetAddr plisten_addr,
                NetAddr clisten_addr,
                hotstuff::pacemaker_bt pmaker,
                const EventContext &ec,
                size_t nworker,
                const Net::Config &repnet_config,
                const ClientNetwork<opcode_t>::Config &clinet_config,
                const std::vector<NetAddr>& _ledger_replicas);

    void start(const std::vector<std::tuple<NetAddr, bytearray_t, bytearray_t>> &reps);
    void stop();

    void commit_set_dump() {
        uint32_t batch_id = 0;

        for (const auto&e: exec_client_rsp) {
            fprintf(stdout, "batch_hash: %s start: %lu, end: %lu\n", get_hex10(e.first).c_str(), e.second.first, e.second.second);
        }


        for (const auto&e: stable_point_errors) {
            fprintf(stdout, "[STABLE POINT ERROR] timestamp: %llu, index: %lu\n", e.first, e.second);
        }
        
        fprintf(stdout, "execution sent: %d, execution count: %d, stable point errors: %d\n", exec_sent, exec_count, stable_point_errors.size());
    }
};

std::pair<std::string, std::string> split_ip_port_cport(const std::string &s) {
    auto ret = trim_all(split(s, ";"));
    if (ret.size() != 2)
        throw std::invalid_argument("invalid cport format");
    return std::make_pair(ret[0], ret[1]);
}

salticidae::BoxObj<HotStuffApp> papp = nullptr;
std::vector<NetAddr> ledger_replicas;

int main(int argc, char **argv) {
    //Config config("hotstuff.conf");
    std::string logfile(argv[2]);
    //std::string ledger_config(argv[3]);
    Config config(argv[1]);

    ElapsedTime elapsed;
    elapsed.start();

    auto opt_blk_size = Config::OptValInt::create(1);
    auto opt_stable_period = Config::OptValInt::create(50);
    auto opt_liveness_delta = Config::OptValInt::create(10);
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
    auto opt_nworker = Config::OptValInt::create(1);
    auto opt_repnworker = Config::OptValInt::create(15);
    auto opt_repburst = Config::OptValInt::create(1000000);
    auto opt_clinworker = Config::OptValInt::create(1);
    auto opt_cliburst = Config::OptValInt::create(1000000);
    auto opt_notls = Config::OptValFlag::create(false);

    config.add_opt("block-size", opt_blk_size, Config::SET_VAL);
    config.add_opt("stable-period", opt_stable_period, Config::SET_VAL);
    config.add_opt("liveness-delta", opt_liveness_delta, Config::SET_VAL);
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
    //config.add_opt("base-timeout", opt_base_timeout, Config::SET_VAL, 't', "set the initial timeout for the Round-Robin Pacemaker");
    config.add_opt("prop-delay", opt_prop_delay, Config::SET_VAL, 't', "set the delay that follows the timeout for the Round-Robin Pacemaker");
    //config.add_opt("imp-timeout", opt_imp_timeout, Config::SET_VAL, 'u', "set impeachment timeout (for sticky)");
    config.add_opt("nworker", opt_nworker, Config::SET_VAL, 'n', "the number of threads for verification");
    config.add_opt("repnworker", opt_repnworker, Config::SET_VAL, 'm', "the number of threads for replica network");
    config.add_opt("repburst", opt_repburst, Config::SET_VAL, 'b', "");
    config.add_opt("clinworker", opt_clinworker, Config::SET_VAL, 'M', "the number of threads for client network");
    config.add_opt("cliburst", opt_cliburst, Config::SET_VAL, 'B', "");
    config.add_opt("notls", opt_notls, Config::SWITCH_ON, 's', "disable TLS");
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

    if (!(0 <= idx && (size_t)idx < replicas.size()))
        throw HotStuffError("replica idx out of range");
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
        .max_msg_size(8192)
        .max_msg_queue_size(6553600)
        .burst_size(opt_repburst->get())
        .nworker(opt_repnworker->get())
        .conn_server_timeout(10)
        .recv_chunk_size(8192)
        .max_recv_buff_size(8192)
        .max_send_buff_size(8192);
    clinet_config
        .max_msg_size(8192)
        .max_msg_queue_size(6553600)
        .burst_size(opt_cliburst->get())
        .nworker(opt_clinworker->get());
    papp = new HotStuffApp(//opt_blk_size->get(),
                           // the batch logic of archipelago is in function client_ordering1_request_cmd_handler in this file
                           1,
                           opt_blk_size->get(),
                           opt_stable_period->get(),
                           opt_liveness_delta->get(),
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
                           clinet_config,
                           ledger_replicas);
    std::vector<std::tuple<NetAddr, bytearray_t, bytearray_t>> reps;
    for (auto &r: replicas)
    {
        auto p = split_ip_port_cport(std::get<0>(r));
        reps.push_back(std::make_tuple(
            NetAddr(p.first),
            hotstuff::from_hex(std::get<1>(r)),
            hotstuff::from_hex(std::get<2>(r))));
    }
    auto shutdown = [&](int) { papp->stop(); };
    salticidae::SigEvent ev_sigint(ec, shutdown);
    salticidae::SigEvent ev_sigterm(ec, shutdown);
    ev_sigint.add(SIGINT);
    ev_sigterm.add(SIGTERM);

    papp->start(reps);
    elapsed.stop(true);

    printf("server%d write to log file %s\n", idx, logfile.c_str());
    freopen(logfile.c_str(), "w", stdout);

    papp->commit_set_dump();

    fclose(stdout);
    return 0;
}

HotStuffApp::HotStuffApp(uint32_t blk_size,
                         uint32_t clnt_blk_size,
                         uint32_t stable_period,
                         uint32_t liveness_delta,
                         double stat_period,
                         double impeach_timeout,
                         ReplicaID _idx,
                         const bytearray_t &raw_privkey,
                         NetAddr plisten_addr,
                         NetAddr clisten_addr,
                         hotstuff::pacemaker_bt pmaker,
                         const EventContext &ec,
                         size_t nworker,
                         const Net::Config &repnet_config,
                         const ClientNetwork<opcode_t>::Config &clinet_config,
                         const std::vector<NetAddr>& _ledger_replicas):
    HotStuff(blk_size, stable_period, liveness_delta, _idx, raw_privkey,
            plisten_addr, std::move(pmaker), ec, nworker, repnet_config),
    idx(_idx), exec_last_batch_clock(0), clnt_blk_size(clnt_blk_size),
    consensus_ready(false),
    ledger_replicas(_ledger_replicas), 
    stat_period(stat_period),
    impeach_timeout(impeach_timeout),
    ec(ec),
    //ledger_network(ec, LedgerNet::Config()),
    cn(req_ec, clinet_config),
    clisten_addr(clisten_addr)
    {
    /* prepare the thread used for sending back confirmations */
    resp_tcall = new salticidae::ThreadCall(resp_ec);
    req_tcall = new salticidae::ThreadCall(req_ec);
    resp_queue.reg_handler(resp_ec, [this](resp_queue_t &q) {
        std::pair<Finality, NetAddr> p;
        while (q.try_dequeue(p))
        {
            try {
                cn.send_msg(MsgRespCmd(std::move(p.first)), p.second);
            } catch (std::exception &err) {
                HOTSTUFF_LOG_WARN("unable to send MsgRespCmd to the client: %s", err.what());
            }
        }
        return false;
    });

    ordering1_queue.reg_handler(resp_ec, [this](ordering1_resp_queue_t &q) {
        std::pair<Ordering1Finality, NetAddr> p;
        while (q.try_dequeue(p))
        {
            try {
                cn.send_msg(MsgOrdering1RespCmd(p.first.cmd_hash, p.first.timestamp, p.first.timestamp_us, p.first.sig), p.second);
            } catch (std::exception &err) {
                HOTSTUFF_LOG_WARN("unable to send MsgOrdering1RespCmd to the client: %s", err.what());
            }
        }
        return false;
    });

    ordering2_queue.reg_handler(resp_ec, [this](ordering2_resp_queue_t &q) {
        std::pair<Ordering2Finality, NetAddr> p;
        while (q.try_dequeue(p))
        {
            try {
                cn.send_msg(MsgOrdering2RespCmd(p.first.cmd_hash, p.first.timestamp, p.first.sig), p.second);
            } catch (std::exception &err) {
                HOTSTUFF_LOG_WARN("unable to send MsgOrdering2RespCmd to the client: %s", err.what());
            }
        }
        return false;
    });

    consensus_queue.reg_handler(resp_ec, [this](consensus_resp_queue_t &q) {
            std::pair<uint256_t, NetAddr> p;
            while (q.try_dequeue(p))
                {
                    try {
                        cn.send_msg(MsgConsensusRespClientCmd(p.first), p.second);
                    } catch (std::exception &err) {
                        HOTSTUFF_LOG_WARN("unable to send MsgConsensusRespClientCmd to the client: %s", err.what());
                    }
                }
            return false;
    });


    
    /* register the handlers for msg from clients */
    cn.reg_handler(salticidae::generic_bind(&HotStuffApp::client_request_cmd_handler, this, _1, _2));

    cn.reg_handler(salticidae::generic_bind(&HotStuffApp::client_ordering1_request_cmd_handler, this, _1, _2));

    cn.reg_handler(salticidae::generic_bind(&HotStuffApp::client_ordering2_request_cmd_handler, this, _1, _2));
    
    cn.start();
    cn.listen(clisten_addr);
}

void HotStuffApp::client_request_cmd_handler(MsgReqCmd &&msg, const conn_t &conn) {
    const NetAddr addr = conn->get_addr();
    auto cmd = parse_cmd(msg.serialized);
    const auto &cmd_hash = cmd->get_hash();
    HOTSTUFF_LOG_DEBUG("processing %s", std::string(*cmd).c_str());
    exec_command(cmd_hash, [this, addr](Finality fin) {
        resp_queue.enqueue(std::make_pair(fin, addr));
    });
}

void HotStuffApp::client_ordering1_request_cmd_handler(MsgOrdering1ReqCmd &&msg, const conn_t &conn) {    
    const NetAddr addr = conn->get_addr();
    auto cmd = parse_cmd(msg.serialized);
    const auto &cmd_hash = cmd->get_hash();

    if (batch_received.count(cmd_hash)) {
        // already in batch
        batch_received[cmd_hash]++;
    } else {
        // new in batch
        batch_received[cmd_hash] = 1;
    }

    if (batch_received[cmd_hash] < clnt_blk_size)
        return;
    
    exec_ordering1(cmd_hash, [this, addr](Ordering1Finality fin) {
        ordering1_queue.enqueue(std::make_pair(fin, addr));
    });
}


void HotStuffApp::client_ordering2_request_cmd_handler(MsgOrdering2ReqCmd &&msg, const conn_t &conn) {
    const NetAddr addr = conn->get_addr();
    uint256_t cmd_hash;
    uint64_t timestamp;
    msg.serialized >> cmd_hash >> timestamp;

    if (timestamp < stable_point) {
        // TODO: implement this code if assert actually happens
        HOTSTUFF_LOG_ERROR("network anomaly detected -- please consider increase the stable-point config parameter");
        assert(false);
    }

    // add to local commit set
    commit_set.push_back(std::make_pair(std::make_pair(cmd_hash, timestamp), addr));

    //HOTSTUFF_LOG_DEBUG("processing %s", std::string(*cmd).c_str());
    exec_ordering2(cmd_hash, [this, addr](Ordering2Finality fin) {
            ordering2_queue.enqueue(std::make_pair(fin, addr));
    });

    // only a single leader starts the consensus phase
    // a dummy implementation that server#1 to be the leader
    if (idx != 1)
        return;
    consensus_ready = true;
    
    // get current time in usec (10^(-6) second)
    struct timespec tmp;
    clock_gettime(CLOCK_MONOTONIC,&tmp);
    uint64_t curr_clock_us=tmp.tv_sec;
    curr_clock_us*=1000*1000;
    curr_clock_us+=tmp.tv_nsec/1000;

    if (exec_last_batch_clock == 0) {
        // start the timer for the first time
        exec_last_batch_clock = curr_clock_us;
    } else if (exec_last_batch_clock + stable_period * 1000 < curr_clock_us) {
        // stable_period milliseconds have passed on timer
        exec_last_batch_clock = curr_clock_us;
        exec_consensus(curr_clock_us, [this](uint256_t cmd_hash, NetAddr addr) {
                consensus_queue.enqueue(std::make_pair(cmd_hash, addr));
        });
    }
}



void HotStuffApp::start(const std::vector<std::tuple<NetAddr, bytearray_t, bytearray_t>> &reps) {
    ev_stat_timer = TimerEvent(ec, [this](TimerEvent &) {
        HotStuff::print_stat();
        HotStuffApp::print_stat();
        //HotStuffCore::prune(100);
        ev_stat_timer.add(stat_period);
    });
    ev_stat_timer.add(stat_period);
    impeach_timer = TimerEvent(ec, [this](TimerEvent &) {
        if (get_decision_waiting().size())
            get_pace_maker()->impeach();
        reset_imp_timer();
    });
    impeach_timer.add(impeach_timeout);


    // only a single leader starts the consensus phase
    // a dummy implementation that server#1 to be the leader
    // if (idx == 1) {
    //     consensus_timer = TimerEvent(ec, [this](TimerEvent &) {
    //             if (consensus_ready) {
    //                 //     struct timespec tmp;
    //                 //     clock_gettime(CLOCK_MONOTONIC,&tmp);
    //                 //     uint64_t curr_clock_us=tmp.tv_sec;
    //                 //     curr_clock_us*=1000*1000;
    //                 //     curr_clock_us+=tmp.tv_nsec/1000;

    //                 // 0 should be curr_clock_us, but it is not used later
    //                 exec_consensus(0, [this](uint256_t cmd_hash, NetAddr addr) {
    //                         consensus_queue.enqueue(std::make_pair(cmd_hash, addr));
    //                     });
    //             }
    //             consensus_timer.del();
    //             consensus_timer.add(stable_period * 1.0 / 1000);
    //         });
    //     consensus_timer.add(stable_period * 1.0 / 1000);
    // }


    
    HOTSTUFF_LOG_INFO("** starting the system with parameters **");
    HOTSTUFF_LOG_INFO("blk_size = %lu", blk_size);
    HOTSTUFF_LOG_INFO("conns = %lu", HotStuff::size());
    HOTSTUFF_LOG_INFO("** starting the event loop...");
    HotStuff::start(reps);
    cn.reg_conn_handler([this](const salticidae::ConnPool::conn_t &_conn, bool connected) {
        auto conn = salticidae::static_pointer_cast<conn_t::type>(_conn);
        if (connected)
            client_conns.insert(conn);
        else
            client_conns.erase(conn);
        return true;
    });
    req_thread = std::thread([this]() { req_ec.dispatch(); });
    resp_thread = std::thread([this]() { resp_ec.dispatch(); });
    /* enter the event main loop */
    ec.dispatch();
}

void HotStuffApp::stop() {
    papp->req_tcall->async_call([this](salticidae::ThreadCall::Handle &) {
        req_ec.stop();
    });
    papp->resp_tcall->async_call([this](salticidae::ThreadCall::Handle &) {
        resp_ec.stop();
    });

    req_thread.join();
    resp_thread.join();
    ec.stop();
}

void HotStuffApp::print_stat() const {
#ifdef HOTSTUFF_MSG_STAT
    HOTSTUFF_LOG_INFO("--- client msg. (10s) ---");
    size_t _nsent = 0;
    size_t _nrecv = 0;
    for (const auto &conn: client_conns)
    {
        if (conn == nullptr) continue;
        size_t ns = conn->get_nsent();
        size_t nr = conn->get_nrecv();
        size_t nsb = conn->get_nsentb();
        size_t nrb = conn->get_nrecvb();
        conn->clear_msgstat();
        HOTSTUFF_LOG_INFO("%s: %u(%u), %u(%u)",
            std::string(conn->get_addr()).c_str(), ns, nsb, nr, nrb);
        _nsent += ns;
        _nrecv += nr;
    }
    HOTSTUFF_LOG_INFO("--- end client msg. ---");
#endif
}
