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

#include "hotstuff/hotstuff.h"
#include "hotstuff/client.h"
#include "hotstuff/liveness.h"

#include <iostream>

#include <sys/time.h>

using salticidae::static_pointer_cast;

#define LOG_INFO HOTSTUFF_LOG_INFO
#define LOG_DEBUG HOTSTUFF_LOG_DEBUG
#define LOG_WARN HOTSTUFF_LOG_WARN

namespace hotstuff {

const opcode_t MsgPropose::opcode;
MsgPropose::MsgPropose(const Proposal &proposal) { serialized << proposal; }
void MsgPropose::postponed_parse(HotStuffCore *hsc) {
    proposal.hsc = hsc;
    serialized >> proposal;
}

const opcode_t MsgVote::opcode;
MsgVote::MsgVote(const Vote &vote) { serialized << vote; }
void MsgVote::postponed_parse(HotStuffCore *hsc) {
    vote.hsc = hsc;
    serialized >> vote;
}

const opcode_t MsgReqBlock::opcode;
MsgReqBlock::MsgReqBlock(const std::vector<uint256_t> &blk_hashes) {
    serialized << htole((uint32_t)blk_hashes.size());
    for (const auto &h: blk_hashes)
        serialized << h;
}

MsgReqBlock::MsgReqBlock(DataStream &&s) {
    uint32_t size;
    s >> size;
    size = letoh(size);
    blk_hashes.resize(size);
    for (auto &h: blk_hashes) s >> h;
}

const opcode_t MsgRespBlock::opcode;
MsgRespBlock::MsgRespBlock(const std::vector<block_t> &blks) {
    serialized << htole((uint32_t)blks.size());
    for (auto blk: blks) serialized << *blk;
}

void MsgRespBlock::postponed_parse(HotStuffCore *hsc) {
    uint32_t size;
    serialized >> size;
    size = letoh(size);
    blks.resize(size);
    for (auto &blk: blks)
    {
        Block _blk;
        _blk.unserialize(serialized, hsc);
        blk = hsc->storage->add_blk(std::move(_blk), hsc->get_config());
    }
}

const opcode_t MsgConsensusReqCmd::opcode;
MsgConsensusReqCmd::MsgConsensusReqCmd(const uint256_t &commit_set_hash, const uint64_t &stable_timestamp, const uint32_t &stable_idx, const uint256_t &place_holder1, const uint256_t &place_holder2, const uint256_t &place_holder3, const uint256_t &place_holder4) {
        serialized << commit_set_hash
                   << stable_timestamp << stable_idx
                   << place_holder1 << place_holder2
                   << place_holder3 << place_holder4
                   << sig;
}
void MsgConsensusReqCmd::postponed_parse() {
    serialized >> commit_set_hash
               >> stable_timestamp >> stable_idx
               >> place_holder1 >> place_holder2
               >> place_holder3 >> place_holder4
               >> sig;
}

    
const opcode_t MsgConsensusRespCmd::opcode;
MsgConsensusRespCmd::MsgConsensusRespCmd(const uint256_t &commit_set_hash, const uint256_t &place_holder1, const uint256_t &place_holder2, const uint256_t &place_holder3, const uint256_t &place_holder4) {
    serialized << commit_set_hash << place_holder1
               << place_holder2 << place_holder3
               << place_holder4 << sig;
}

void MsgConsensusRespCmd::postponed_parse() {
    serialized >> commit_set_hash >> place_holder1
      >> place_holder2 >> place_holder3
      >> place_holder4 >> sig;
}



// TODO: improve this function
void HotStuffBase::exec_command(uint256_t cmd_hash, commit_cb_t callback) {
    cmd_pending.enqueue(std::make_pair(cmd_hash, callback));
}

void HotStuffBase::exec_ordering1(uint256_t cmd_hash, ordering1_cb_t callback){
    ordering1.enqueue(std::make_pair(cmd_hash, callback));
}

void HotStuffBase::exec_ordering2(uint256_t cmd_hash, ordering2_cb_t callback){
    ordering2.enqueue(std::make_pair(cmd_hash, callback));
}

void HotStuffBase::exec_consensus(uint64_t timestamp, consensus_cb_t callback)
{
    consensus.enqueue(std::make_pair(timestamp, callback));
}

void HotStuffBase::broadcast_start_consensus(const uint256_t& commit_set_hash, uint64_t stable_timestamp, uint32_t stable_idx, const uint256_t& cmd_hash1, const uint256_t& cmd_hash2, const uint256_t& cmd_hash3, const uint256_t& cmd_hash4) {
    // send the start consensus request to all servers
    //printf("leader broadcast %.10s\n", get_hex(commit_set_hash).c_str());
    // printf("######## broadcast_start_consensus %s %s %s %s\n",
    //        get_hex(commit_set_hash).c_str(),
    //        get_hex(cmd_hash2).c_str(),
    //        get_hex(cmd_hash3).c_str(),
    //        get_hex(cmd_hash4).c_str());

    pn.multicast_msg(MsgConsensusReqCmd(commit_set_hash, stable_timestamp, stable_idx, cmd_hash1, cmd_hash2, cmd_hash3, cmd_hash4), peers);
}

void HotStuffBase::resp_start_consensus(const uint256_t &commit_set_hash, const uint256_t& cmd_hash1, const uint256_t& cmd_hash2, const uint256_t& cmd_hash3, const uint256_t& cmd_hash4, const PeerId leader_addr) {
    pn.send_msg(MsgConsensusRespCmd(commit_set_hash, cmd_hash1, cmd_hash2, cmd_hash3, cmd_hash4), leader_addr);
}



void HotStuffBase::on_fetch_blk(const block_t &blk) {
#ifdef HOTSTUFF_BLK_PROFILE
    blk_profiler.get_tx(blk->get_hash());
#endif
    LOG_DEBUG("fetched %.10s", get_hex(blk->get_hash()).c_str());
    part_fetched++;
    fetched++;
    //for (auto cmd: blk->get_cmds()) on_fetch_cmd(cmd);
    const uint256_t &blk_hash = blk->get_hash();
    auto it = blk_fetch_waiting.find(blk_hash);
    if (it != blk_fetch_waiting.end())
    {
        it->second.resolve(blk);
        blk_fetch_waiting.erase(it);
    }
}

bool HotStuffBase::on_deliver_blk(const block_t &blk) {
    const uint256_t &blk_hash = blk->get_hash();
    bool valid;
    /* sanity check: all parents must be delivered */
    for (const auto &p: blk->get_parent_hashes())
        assert(storage->is_blk_delivered(p));
    if ((valid = HotStuffCore::on_deliver_blk(blk)))
    {
        LOG_DEBUG("block %.10s delivered",
                get_hex(blk_hash).c_str());
        part_parent_size += blk->get_parent_hashes().size();
        part_delivered++;
        delivered++;
    }
    else
    {
        LOG_WARN("dropping invalid block");
    }

    bool res = true;
    auto it = blk_delivery_waiting.find(blk_hash);
    if (it != blk_delivery_waiting.end())
    {
        auto &pm = it->second;
        if (valid)
        {
            pm.elapsed.stop(false);
            auto sec = pm.elapsed.elapsed_sec;
            part_delivery_time += sec;
            part_delivery_time_min = std::min(part_delivery_time_min, sec);
            part_delivery_time_max = std::max(part_delivery_time_max, sec);

            pm.resolve(blk);
        }
        else
        {
            pm.reject(blk);
            res = false;
            // TODO: do we need to also free it from storage?
        }
        blk_delivery_waiting.erase(it);
    }
    return res;
}

promise_t HotStuffBase::async_fetch_blk(const uint256_t &blk_hash,
                                        const PeerId *replica,
                                        bool fetch_now) {
    if (storage->is_blk_fetched(blk_hash))
        return promise_t([this, &blk_hash](promise_t pm){
            pm.resolve(storage->find_blk(blk_hash));
        });
    auto it = blk_fetch_waiting.find(blk_hash);
    if (it == blk_fetch_waiting.end())
    {
#ifdef HOTSTUFF_BLK_PROFILE
        blk_profiler.rec_tx(blk_hash, false);
#endif
        it = blk_fetch_waiting.insert(
            std::make_pair(
                blk_hash,
                BlockFetchContext(blk_hash, this))).first;
    }
    if (replica != nullptr)
        it->second.add_replica(*replica, fetch_now);
    return static_cast<promise_t &>(it->second);
}

promise_t HotStuffBase::async_deliver_blk(const uint256_t &blk_hash,
                                        const PeerId &replica) {
    if (storage->is_blk_delivered(blk_hash))
        return promise_t([this, &blk_hash](promise_t pm) {
            pm.resolve(storage->find_blk(blk_hash));
        });
    auto it = blk_delivery_waiting.find(blk_hash);
    if (it != blk_delivery_waiting.end())
        return static_cast<promise_t &>(it->second);
    BlockDeliveryContext pm{[](promise_t){}};
    it = blk_delivery_waiting.insert(std::make_pair(blk_hash, pm)).first;
    /* otherwise the on_deliver_batch will resolve */
    async_fetch_blk(blk_hash, &replica).then([this, replica](block_t blk) {
        /* qc_ref should be fetched */
        std::vector<promise_t> pms;
        const auto &qc = blk->get_qc();
        assert(qc);
        if (blk == get_genesis())
            pms.push_back(promise_t([](promise_t &pm){ pm.resolve(true); }));
        else
            pms.push_back(blk->verify(this, vpool));
        pms.push_back(async_fetch_blk(qc->get_obj_hash(), &replica));
        /* the parents should be delivered */
        for (const auto &phash: blk->get_parent_hashes())
            pms.push_back(async_deliver_blk(phash, replica));
        promise::all(pms).then([this, blk](const promise::values_t values) {
            auto ret = promise::any_cast<bool>(values[0]) && this->on_deliver_blk(blk);
            if (!ret)
                HOTSTUFF_LOG_WARN("verification failed during async delivery");
        });
    });
    return static_cast<promise_t &>(pm);
}

void HotStuffBase::propose_handler(MsgPropose &&msg, const Net::conn_t &conn) {
    const PeerId &peer = conn->get_peer_id();
    if (peer.is_null()) return;
    msg.postponed_parse(this);
    auto &prop = msg.proposal;
    block_t blk = prop.blk;

    if (!blk) return;
    promise::all(std::vector<promise_t>{
        async_deliver_blk(blk->get_hash(), peer)
    }).then([this, prop = std::move(prop)]() {
        on_receive_proposal(prop);
    });
}

void HotStuffBase::vote_handler(MsgVote &&msg, const Net::conn_t &conn) {
    const auto &peer = conn->get_peer_id();
    if (peer.is_null()) return;
    msg.postponed_parse(this);
    //auto &vote = msg.vote;
    RcObj<Vote> v(new Vote(std::move(msg.vote)));
    promise::all(std::vector<promise_t>{
        async_deliver_blk(v->blk_hash, peer),
        v->verify(vpool),
    }).then([this, v=std::move(v)](const promise::values_t values) {
        if (!promise::any_cast<bool>(values[1]))
            LOG_WARN("invalid vote from %d", v->voter);
        else
            on_receive_vote(*v);
    });
}

void HotStuffBase::req_blk_handler(MsgReqBlock &&msg, const Net::conn_t &conn) {
    const PeerId replica = conn->get_peer_id();
    if (replica.is_null()) return;
    auto &blk_hashes = msg.blk_hashes;
    std::vector<promise_t> pms;
    for (const auto &h: blk_hashes)
        pms.push_back(async_fetch_blk(h, nullptr));
    promise::all(pms).then([replica, this](const promise::values_t values) {
        std::vector<block_t> blks;
        for (auto &v: values)
        {
            auto blk = promise::any_cast<block_t>(v);
            blks.push_back(blk);
        }
        pn.send_msg(MsgRespBlock(blks), replica);
    });
}

void HotStuffBase::resp_blk_handler(MsgRespBlock &&msg, const Net::conn_t &) {
    msg.postponed_parse(this);
    for (const auto &blk: msg.blks)
        if (blk) on_fetch_blk(blk);
}

bool HotStuffBase::conn_handler(const salticidae::ConnPool::conn_t &conn, bool connected) {
    if (connected)
    {
        auto cert = conn->get_peer_cert();
        //SALTICIDAE_LOG_INFO("%s", salticidae::get_hash(cert->get_der()).to_hex().c_str());
        return (!cert) || valid_tls_certs.count(salticidae::get_hash(cert->get_der()));
    }
    return true;
}

void HotStuffBase::print_stat() const {
    LOG_INFO("===== begin stats =====");
    LOG_INFO("-------- queues -------");
    LOG_INFO("blk_fetch_waiting: %lu", blk_fetch_waiting.size());
    LOG_INFO("blk_delivery_waiting: %lu", blk_delivery_waiting.size());
    LOG_INFO("decision_waiting: %lu", decision_waiting.size());
    LOG_INFO("-------- misc ---------");
    LOG_INFO("fetched: %lu", fetched);
    LOG_INFO("delivered: %lu", delivered);
    LOG_INFO("cmd_cache: %lu", storage->get_cmd_cache_size());
    LOG_INFO("blk_cache: %lu", storage->get_blk_cache_size());
    LOG_INFO("------ misc (10s) -----");
    LOG_INFO("fetched: %lu", part_fetched);
    LOG_INFO("delivered: %lu", part_delivered);
    LOG_INFO("decided: %lu", part_decided);
    LOG_INFO("gened: %lu", part_gened);
    LOG_INFO("avg. parent_size: %.3f",
            part_delivered ? part_parent_size / double(part_delivered) : 0);
    LOG_INFO("delivery time: %.3f avg, %.3f min, %.3f max",
            part_delivered ? part_delivery_time / double(part_delivered) : 0,
            part_delivery_time_min == double_inf ? 0 : part_delivery_time_min,
            part_delivery_time_max);

    part_parent_size = 0;
    part_fetched = 0;
    part_delivered = 0;
    part_decided = 0;
    part_gened = 0;
    part_delivery_time = 0;
    part_delivery_time_min = double_inf;
    part_delivery_time_max = 0;
#ifdef HOTSTUFF_MSG_STAT
    LOG_INFO("--- replica msg. (10s) ---");
    size_t _nsent = 0;
    size_t _nrecv = 0;
    for (const auto &replica: peers)
    {
        auto conn = pn.get_peer_conn(replica);
        if (conn == nullptr) continue;
        size_t ns = conn->get_nsent();
        size_t nr = conn->get_nrecv();
        size_t nsb = conn->get_nsentb();
        size_t nrb = conn->get_nrecvb();
        conn->clear_msgstat();
        LOG_INFO("%s: %u(%u), %u(%u), %u",
            get_hex10(replica).c_str(), ns, nsb, nr, nrb, part_fetched_replica[replica]);
        _nsent += ns;
        _nrecv += nr;
        part_fetched_replica[replica] = 0;
    }
    nsent += _nsent;
    nrecv += _nrecv;
    LOG_INFO("sent: %lu", _nsent);
    LOG_INFO("recv: %lu", _nrecv);
    LOG_INFO("--- replica msg. total ---");
    LOG_INFO("sent: %lu", nsent);
    LOG_INFO("recv: %lu", nrecv);
#endif
    LOG_INFO("====== end stats ======");
}

HotStuffBase::HotStuffBase(uint32_t blk_size,
                           uint32_t stable_period,
                           uint32_t liveness_delta,
                    ReplicaID rid,
                    privkey_bt &&priv_key,
                    NetAddr listen_addr,
                    pacemaker_bt pmaker,
                    EventContext ec,
                    size_t nworker,
                    const Net::Config &netconfig):
        HotStuffCore(rid, std::move(priv_key)),
        stable_point(0),
        stable_point_idx(0),
        cmd_cnt(0),
        stable_period(stable_period),
        liveness_delta(liveness_delta),
        exec_count(0), exec_sent(0),
        listen_addr(listen_addr),
        blk_size(blk_size),
        ec(ec),
        tcall(ec),
        vpool(ec, nworker),
        pn(ec, netconfig),
        pmaker(std::move(pmaker)),

        fetched(0), delivered(0),
        nsent(0), nrecv(0),
        part_parent_size(0),
        part_fetched(0),
        part_delivered(0),
        part_decided(0),
        part_gened(0),
        part_delivery_time(0),
        part_delivery_time_min(double_inf),
        part_delivery_time_max(0)
{
    /* register the handlers for msg from replicas */
    pn.reg_handler(salticidae::generic_bind(&HotStuffBase::propose_handler, this, _1, _2));
    pn.reg_handler(salticidae::generic_bind(&HotStuffBase::vote_handler, this, _1, _2));
    pn.reg_handler(salticidae::generic_bind(&HotStuffBase::req_blk_handler, this, _1, _2));
    pn.reg_handler(salticidae::generic_bind(&HotStuffBase::resp_blk_handler, this, _1, _2));


    pn.reg_handler(salticidae::generic_bind(&HotStuffBase::server_consensus_request_cmd_handler, this, _1, _2));

    //pn.reg_handler(salticidae::generic_bind(&HotStuffBase::server_consensus_reponse_cmd_handler, this, _1, _2));

    pn.reg_conn_handler(salticidae::generic_bind(&HotStuffBase::conn_handler, this, _1, _2));


    pn.start();
    pn.listen(listen_addr);
}

void HotStuffBase::check_stable_point_index(uint256_t commit_set_hash, uint64_t stable_timestamp, uint32_t stable_idx) {
    // a dummy implementation that only checks the time interval of the batch and the number of commands in the batch

    if (stable_point_idx < stable_idx) {
        std::sort(commit_set.begin() + stable_point_idx, commit_set.end(), commit_set_cmp);
        stable_point_idx = stable_idx;
    }

    uint32_t commit_set_size = commit_set.size();
    if (stable_idx < commit_set_size) {
        if (commit_set[stable_idx].first.second > stable_timestamp) {
            if (stable_idx > 0 && commit_set[stable_idx - 1].first.second <= stable_timestamp) {
                // essentially, commit_set[0, stable_idx) are all commands in the commit set with a timestamp <= stable_timestamp
                return;
            }
        }
    }

    // error detected, log the error
    stable_point_errors.push_back(std::make_pair(stable_timestamp, stable_idx));    
}

void HotStuffBase::server_consensus_request_cmd_handler(MsgConsensusReqCmd &&msg, const Net::conn_t &conn) {
    //const PeerId leader = conn->get_peer_id();
    msg.postponed_parse();

    // a dummy implementation that server#1 to be the leader
    if (get_id() == 1)
        return;

    consensus_nonleader.enqueue(msg);
}

void HotStuffBase::server_consensus_reponse_cmd_handler(MsgConsensusRespCmd &&msg, const Net::conn_t &) {
    // only the leader receives response
    // a dummy implementation that server#1 to be the leader
    if (get_id() != 1)
        return;

    msg.postponed_parse();

    if (leader_propose.count(msg.commit_set_hash) > 0) {
        // propose only once, but multiple servers will reply and hence call this funciton
        return;
    }

    leader_propose.insert(msg.commit_set_hash);
}


void HotStuffBase::do_broadcast_proposal(const Proposal &prop) {
    pn.multicast_msg(MsgPropose(prop), peers);
}

void HotStuffBase::do_vote(ReplicaID last_proposer, const Vote &vote) {
    pmaker->beat_resp(last_proposer)
            .then([this, vote](ReplicaID proposer) {
        if (proposer == get_id())
        {
            throw HotStuffError("unreachable line");
            //on_receive_vote(vote);
        }
        else
            pn.send_msg(MsgVote(vote), get_config().get_peer_id(proposer));
    });
}

void HotStuffBase::do_consensus(const block_t &blk) {
    pmaker->on_consensus(blk);
}

void HotStuffBase::do_decide(Finality &&fin) {
    part_decided++;
    state_machine_execute(fin);
    auto it = decision_waiting.find(fin.cmd_hash);
    if (it != decision_waiting.end())
    {
        //it->second(std::move(fin));
        exec_pending.enqueue(std::make_pair(it->second, std::move(fin)));
        decision_waiting.erase(it);
    } else {
        // std::cout << "hotstuff do_decide not finding cmd_hash at height" << fin.cmd_height << std::endl;
        decision_made[fin.cmd_hash] = fin.cmd_height;
    }
}

HotStuffBase::~HotStuffBase() {}

void HotStuffBase::start(
        std::vector<std::tuple<NetAddr, pubkey_bt, uint256_t>> &&replicas,
        bool ec_loop) {
    for (size_t i = 0; i < replicas.size(); i++)
    {
        auto &addr = std::get<0>(replicas[i]);
        auto cert_hash = std::move(std::get<2>(replicas[i]));
        valid_tls_certs.insert(cert_hash);
        salticidae::PeerId peer{cert_hash};
        HotStuffCore::add_replica(i, peer, std::move(std::get<1>(replicas[i])));
        if (addr != listen_addr)
        {
            peers.push_back(peer);
            pn.add_peer(peer);
            pn.set_peer_addr(peer, addr);
            pn.conn_peer(peer);
        }
    }

    /* ((n - 1) + 1 - 1) / 3 */
    uint32_t nfaulty = peers.size() / 3;
    if (nfaulty == 0)
        LOG_WARN("too few replicas in the system to tolerate any failure");
    on_init(nfaulty);
    pmaker->init(this);
    if (ec_loop)
        ec.dispatch();

    consensus_nonleader.reg_handler(ec, [this](consensus_nonleader_queue_t &q) {
            MsgConsensusReqCmd msg;
            while (q.try_dequeue(msg)) {
                // check msg.stable_timestamp and msg.stable_idx
                // optimistically assumes the check passes but errors will be logged if any
                check_stable_point_index(msg.commit_set_hash, msg.stable_timestamp, msg.stable_idx);

                //printf("######## nonleader exec_command\n");
                exec_command(msg.commit_set_hash, [this](Finality fin) {});
                exec_command(msg.place_holder2, [this](Finality fin) {});
                exec_command(msg.place_holder3, [this](Finality fin) {});
                exec_command(msg.place_holder4, [this](Finality fin) {});

                return true;
            }
            return false;
        });

    consensus.reg_handler(ec, [this](consensus_queue_t &q) {
             std::pair<uint64_t, consensus_cb_t> e;
             // e.first is the timestamp for the consensus phase
             // e.second is the callback function for client notification
             while (q.try_dequeue(e)) {
                 // the pipeline design of HotStuff requires 4 commands to be issued at the same time for the first command to commit -- the following definitions are place-holder commands pushing the actual consensus command in the protocol to commit.        
                 auto cmd_hash1 = CommandDummy(0, cmd_cnt++).get_hash();
                 auto cmd_hash2 = CommandDummy(1, cmd_cnt++).get_hash();
                 auto cmd_hash3 = CommandDummy(2, cmd_cnt++).get_hash();        
                 auto cmd_hash4 = CommandDummy(3, cmd_cnt++).get_hash();

                 uint32_t commit_set_size = commit_set.size();
                 std::sort(commit_set.begin() + stable_point_idx, commit_set.end(), commit_set_cmp);
                 uint32_t next_stable_point_idx = stable_point_idx;
                 uint64_t batch_end_timestamp = commit_set[commit_set_size - 1].first.second - liveness_delta * 1000;
                 for(; next_stable_point_idx < commit_set_size; next_stable_point_idx++) {
                     if (commit_set[next_stable_point_idx].first.second > batch_end_timestamp)
                         break;
                 }
                 uint32_t start = stable_point_idx;
                 uint64_t end = next_stable_point_idx;
                 // stable_point = commit_set[stable_point].first.second;
                 stable_point_idx = next_stable_point_idx;

                 // a dummy implementation that only checks the time interval of the batch and the number of commands in the batch
                 auto commit_set_hash = CommandDummy(4, cmd_cnt++).get_hash();
                 // send the consensus request to other servers
                 broadcast_start_consensus(commit_set_hash, batch_end_timestamp, next_stable_point_idx, cmd_hash1, cmd_hash2, cmd_hash3, cmd_hash4);

                 exec_client_rsp[commit_set_hash] = std::make_pair(start, end);
                 exec_sent = end;

                 exec_command(commit_set_hash, [this, e, commit_set_hash](Finality fin) {
                         uint32_t start = exec_client_rsp[commit_set_hash].first;
                         uint32_t end = exec_client_rsp[commit_set_hash].second;

                         for (uint32_t i = start; i < end; i++) {
                             e.second(commit_set[i].first.first, commit_set[i].second);
                         }

                         if (exec_count < end)
                             exec_count = end;
                     });

                 // place-holder cmd2
                 //exec_command_noresp(cmd_hash2);
                 //cmd_noresp_pending.enqueue(cmd_hash2);
                 exec_command(cmd_hash2, [this](Finality fin) {});
                 // place-holder cmd3
                 //exec_command_noresp(cmd_hash3);
                 //cmd_noresp_pending.enqueue(cmd_hash3);
                 exec_command(cmd_hash3, [this](Finality fin) {});
                 // place-holder cmd4
                 //exec_command_noresp(cmd_hash4);
                 //cmd_noresp_pending.enqueue(cmd_hash4);
                 exec_command(cmd_hash4, [this](Finality fin) {});

                 return true;
             }
             return false;
     });

    exec_pending.reg_handler(ec, [this](exec_queue_t &q) {
        std::pair<commit_cb_t, Finality> e;
        while (q.try_dequeue(e))
        {
            // execute the command
            e.first(e.second);
        }
        return false;
    });

    cmd_pending.reg_handler(ec, [this](cmd_queue_t &q) {
        std::pair<uint256_t, commit_cb_t> e;
        while (q.try_dequeue(e))
        {
            ReplicaID proposer = pmaker->get_proposer();
            const auto &cmd_hash = e.first;

            // Fix bug triggered by Indicus
            if (decision_made.count(cmd_hash)) {
                // command has been committed
                uint32_t height = decision_made[cmd_hash];
                //e.second(Finality(id, 0, 0, height, cmd_hash, uint256_t()));
                exec_pending.enqueue(std::make_pair(e.second, Finality(id, 0, 0, height, cmd_hash, uint256_t())));
                continue;
            }
            
            auto it = decision_waiting.find(cmd_hash);

            if (it == decision_waiting.end())
                it = decision_waiting.insert(std::make_pair(cmd_hash, e.second)).first;
            else
                //e.second(Finality(id, 0, 0, 0, cmd_hash, uint256_t()));
                exec_pending.enqueue(std::make_pair(e.second, Finality(id, 0, 0, 0, cmd_hash, uint256_t())));

            if (proposer != get_id()) continue;
            cmd_pending_buffer.push(cmd_hash);
            if (cmd_pending_buffer.size() >= blk_size)
            {
                std::vector<uint256_t> cmds;
                for (uint32_t i = 0; i < blk_size; i++)
                {
                    cmds.push_back(cmd_pending_buffer.front());
                    cmd_pending_buffer.pop();
                }
                pmaker->beat().then([this, cmds = std::move(cmds)](ReplicaID proposer) {

                    if (proposer == get_id())
                        on_propose(cmds, pmaker->get_parents());
                });

                return true;
            }
        }
        return false;
    });

     ordering1.reg_handler(ec, [this](ordering1_queue_t &q) {
         std::pair<uint256_t, ordering1_cb_t> e;
         
         while (q.try_dequeue(e))
         {
             // assgin timestamp
             struct timeval tv;
             gettimeofday(&tv, nullptr);
             uint64_t timestamp_us = tv.tv_sec;
             timestamp_us *= 1000 * 1000;
             timestamp_us += tv.tv_usec;

             // encrypt timestamp
             // a dummy implementation using a fixed crypto key
             PrivKeySecp256k1 p;
             p.from_hex("4aede145d13021fb43c938bced67511a7740c05786d3e0b94ffbdaa7f15afc57");
             uint8_t timestamp[32] = "0";
             *((uint64_t*)timestamp) = timestamp_us;
             SigSecp256k1 sig(timestamp, p);
             DataStream s;
             sig.serialize(s);

             e.second(Ordering1Finality(e.first, timestamp, timestamp_us, sig));
             return true;
         }
         return false;
    });


     ordering2.reg_handler(ec, [this](ordering2_queue_t &q) {
         std::pair<uint256_t, ordering2_cb_t> e;

         while (q.try_dequeue(e))
         {
             uint8_t timestamp[32] = "00";
             std::vector<unsigned char> time_vec(timestamp, timestamp+32);

             // encrypt timestamp
             PrivKeySecp256k1 p;
             p.from_hex("4aede145d13021fb43c938bced67511a7740c05786d3e0b94ffbdaa7f15afc57");
             SigSecp256k1 sig(timestamp, p);
             DataStream s;
             sig.serialize(s);
             

             // verify 2f+1 sigs
             // pubkey_bt pub = p.get_pubkey();
              pubkey_bt pub = p.get_pubkey();
              DataStream s1;
              s1 << *pub;
              PubKeySecp256k1 pub2;
              s1 >> pub2;

              s1 << sig;
              SigSecp256k1 sig2(secp256k1_default_verify_ctx);
              s1 >> sig2;

              // a dummy implementation simulating the workload of decrypting 2f+1 timestamps
              uint32_t nfaulty = peers.size() / 3;
              for (int i = 0; i < 2*nfaulty+1; i++) {
                  sig2.verify(time_vec, pub2);
              }
             
             e.second(Ordering2Finality(e.first, timestamp, sig));
             return true;
         }
         return false;
    });

}

}
