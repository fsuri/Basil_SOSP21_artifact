/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fsp@cs.cornell.edu>
 *                Zheng Wang <zw494@cornell.edu>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************************/
#ifndef HOTSTUFF_COMMON_H
#define HOTSTUFF_COMMON_H

#include "lib/configuration.h"
#include "lib/keymanager.h"
#include "store/common/timestamp.h"
#include "store/bftsmartstore/pbft-proto.pb.h"
#include "store/bftsmartstore/server-proto.pb.h"
#include "lib/transport.h"

#include <map>
#include <string>
#include <vector>
#include <condition_variable>
#include <mutex>
#include <google/protobuf/message.h>

namespace bftsmartstore {

  struct asyncVerification{
    asyncVerification(int no_groups) : groupTotals(no_groups), result(false) { }
    ~asyncVerification() { deleteMessages();}

    std::mutex objMutex;
    std::mutex blockingMutex;
    std::condition_variable cv_wait;

    std::vector<std::string*> sigMsgs;

    void deleteMessages(){
      for(auto sigMsg : sigMsgs){
        delete(sigMsg);//delete ccMsg;
      }
    }

    uint32_t quorumSize;


    std::map<uint64_t, uint32_t> groupCounts;
    int groupTotals;
    int groupsVerified = 0;

    bool result;
    int deletable;
    bool terminate;
  };

bool ValidateSignedMessage(const proto::SignedMessage &signedMessage,
    KeyManager *keyManager, ::google::protobuf::Message &plaintextMsg);

bool ValidateSignedMessage(const proto::SignedMessage &signedMessage,
    KeyManager *keyManager, std::string &data, std::string &type);

bool __PreValidateSignedMessage(const proto::SignedMessage &signedMessage,
    KeyManager *keyManager, proto::PackedMessage &packedMessage);

bool CheckSignature(const proto::SignedMessage &signedMessage,
    KeyManager *keyManager);

void SignMessage(const ::google::protobuf::Message &msg,
    crypto::PrivKey* privateKey, uint64_t processId,
    proto::SignedMessage &signedMessage);

std::string TransactionDigest(const proto::Transaction &txn);

std::string BatchedDigest(proto::BatchedRequest& breq);

std::string string_to_hex(const std::string& input);

void DebugHash(const std::string& hash);

// return true if the grouped decision is valid
bool verifyGDecision(const proto::GroupedDecision& gdecision,
  const proto::Transaction& txn, KeyManager* keyManager, bool signMessages, uint64_t f);

bool verifyG_Abort_Decision(const proto::GroupedDecision& gdecision,
  const proto::Transaction& txn, KeyManager* keyManager, bool signMessages, uint64_t f);

bool verifyGDecision_parallel(const proto::GroupedDecision& gdecision,
  const proto::Transaction& txn, KeyManager* keyManager, bool signMessages, uint64_t f, Transport* tp);

bool verifyGDecision_Abort_parallel(const proto::GroupedDecision& gdecision,
  const proto::Transaction& txn, KeyManager* keyManager, bool signMessages, uint64_t f, Transport* tp );

std::string BytesToHex(const std::string &bytes, size_t maxLength);

} // namespace bftsmartstore

#endif /* HOTSTUFF_COMMON_H */
