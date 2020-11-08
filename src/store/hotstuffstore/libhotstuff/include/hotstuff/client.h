/**
 * Copyright 2018 VMware
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

#ifndef _HOTSTUFF_CLIENT_H
#define _HOTSTUFF_CLIENT_H

#include "salticidae/msg.h"
#include "hotstuff/type.h"
#include "hotstuff/entity.h"
#include "hotstuff/consensus.h"

namespace hotstuff {

struct MsgReqCmd {
    static const opcode_t opcode = 0x7;
    DataStream serialized;
    command_t cmd;
    MsgReqCmd(const Command &cmd) { serialized << cmd; }
    MsgReqCmd(DataStream &&s): serialized(std::move(s)) {}
};

struct MsgRespCmd {
    static const opcode_t opcode = 0x8;
    DataStream serialized;
#if HOTSTUFF_CMD_RESPSIZE > 0
    uint8_t payload[HOTSTUFF_CMD_RESPSIZE];
#endif
    Finality fin;
    MsgRespCmd(const Finality &fin) {
        serialized << fin;
#if HOTSTUFF_CMD_RESPSIZE > 0
        serialized.put_data(payload, payload + sizeof(payload));
#endif
    }
    MsgRespCmd(DataStream &&s) {
        s >> fin;
    }
};

struct MsgOrdering1ReqCmd {
    static const opcode_t opcode = 0x9;
    DataStream serialized;
    command_t cmd;
    MsgOrdering1ReqCmd(const Command &cmd) { serialized << cmd; }
    MsgOrdering1ReqCmd(DataStream &&s): serialized(std::move(s)) {}
};

struct MsgOrdering1RespCmd {
    static const opcode_t opcode = 0xa;
    DataStream serialized;
    uint256_t cmd_hash, timestamp;
    uint64_t timestamp_us;
    SigSecp256k1 sig;

    MsgOrdering1RespCmd(const uint256_t &cmd_hash, const uint256_t &timestamp, const uint64_t timestamp_us, const SigSecp256k1 &sig) {
        serialized << cmd_hash << timestamp << sig << timestamp_us;
    }
    MsgOrdering1RespCmd(DataStream &&s) {
        s >> cmd_hash >> timestamp >> sig >> timestamp_us;
    }
};

// a dummy implementation that carries fixed number of signatures
// we choose 33 because we use at most 100 machines and 3*33+1=100
const int MAX_FAILURES = 33;
struct MsgOrdering2ReqCmd {
    static const opcode_t opcode = 0xb;
    DataStream serialized;
    uint256_t cmd_hash;
    uint64_t timestamp;
    SigSecp256k1 sig[MAX_FAILURES * 2 + 1];
    MsgOrdering2ReqCmd(const uint256_t &cmd_hash, uint64_t timestamp) {
        serialized << cmd_hash << timestamp;
        /* for (int i = 0; i < MAX_FAILURES * 2 + 1; i++) */
        /*     serialized << sig[i]; */
    }
    MsgOrdering2ReqCmd(DataStream &&s): serialized(std::move(s)) {}
};

struct MsgOrdering2RespCmd {
    static const opcode_t opcode = 0xc;
    DataStream serialized;
    uint256_t cmd_hash, timestamp;
    SigSecp256k1 sig;

    MsgOrdering2RespCmd(const uint256_t &cmd_hash, const uint256_t &timestamp, const SigSecp256k1 &sig) {
        serialized << cmd_hash << timestamp << sig;
    }
    MsgOrdering2RespCmd(DataStream &&s) {
        s >> cmd_hash >> timestamp >> sig;
    }
};


struct MsgConsensusRespClientCmd {
    static const opcode_t opcode = 0xd;
    DataStream serialized;
    uint256_t cmd_hash;
    SigSecp256k1 sig;

    MsgConsensusRespClientCmd(const uint256_t &cmd_hash) {
        serialized << cmd_hash <<  sig;
    }
    MsgConsensusRespClientCmd(DataStream &&s) {
        s >> cmd_hash >> sig;
    }
};



//#ifdef HOTSTUFF_AUTOCLI
//struct MsgDemandCmd {
//    static const opcode_t opcode = 0x6;
//    DataStream serialized;
//    size_t ncmd;
//    MsgDemandCmd(size_t ncmd) { serialized << ncmd; }
//    MsgDemandCmd(DataStream &&s) { s >> ncmd; }
//};
//#endif

class CommandDummy: public Command {
    uint32_t cid;
    uint32_t n;
    uint256_t hash;
#if HOTSTUFF_CMD_REQSIZE > 0
    uint8_t payload[HOTSTUFF_CMD_REQSIZE];
#endif

    public:
    CommandDummy() {}
    ~CommandDummy() override {}

    CommandDummy(uint32_t cid, uint32_t n):
        cid(cid), n(n), hash(salticidae::get_hash(*this)) {}

    void serialize(DataStream &s) const override {
        s << cid << n;
#if HOTSTUFF_CMD_REQSIZE > 0
        s.put_data(payload, payload + sizeof(payload));
#endif
    }

    void unserialize(DataStream &s) override {
        s >> cid >> n;
#if HOTSTUFF_CMD_REQSIZE > 0
        auto base = s.get_data_inplace(HOTSTUFF_CMD_REQSIZE);
        memmove(payload, base, sizeof(payload));
#endif
        hash = salticidae::get_hash(*this);
    }

    const uint256_t &get_hash() const override {
        return hash;
    }

    bool verify() const override {
        return true;
    }
};

}

#endif
