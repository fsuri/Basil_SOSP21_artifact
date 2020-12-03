#ifndef HOTSTUFF_COMMON_H
#define HOTSTUFF_COMMON_H

#include "lib/configuration.h"
#include "lib/keymanager.h"
#include "store/common/timestamp.h"
#include "store/hotstuffstore/pbft-proto.pb.h"
#include "store/hotstuffstore/server-proto.pb.h"
#include "lib/transport.h"

#include <map>
#include <string>
#include <vector>
#include <condition_variable>
#include <mutex>
#include <google/protobuf/message.h>

namespace hotstuffstore {

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

} // namespace hotstuffstore

#endif /* HOTSTUFF_COMMON_H */
