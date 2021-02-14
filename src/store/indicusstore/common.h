#ifndef INDICUS_COMMON_H
#define INDICUS_COMMON_H

#include "lib/configuration.h"
#include "lib/keymanager.h"
#include "store/common/timestamp.h"
#include "store/indicusstore/indicus-proto.pb.h"
#include "lib/latency.h"
#include "store/indicusstore/verifier.h"
#include "lib/tcptransport.h"

#include <map>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include "tbb/concurrent_vector.h"

#include <google/protobuf/message.h>


namespace indicusstore {



static bool LocalDispatch = true; //TODO: Turn into config flag if a viable option.

typedef std::function<void()> signedCallback;
typedef std::function<void()> cleanCallback;
//typedef std::function<void(void*)> verifyCallback;
typedef std::function<void(void*)> mainThreadCallback; //TODO change back to this...
//typedef std::function<void(bool)> mainThreadCallback;

struct Triplet {
  Triplet() {};
  Triplet(::google::protobuf::Message* msg,
  proto::SignedMessage* sig_msg,
  signedCallback cb) : msg(msg), sig_msg(sig_msg), cb(cb) { };
  ~Triplet() { };
  ::google::protobuf::Message* msg;
  proto::SignedMessage* sig_msg;
  signedCallback cb;
};



//static bool True = true;
//static bool False = false;

static std::vector<std::string*> MessageStrings;
static std::mutex msgStr_mutex;
std::string* GetUnusedMessageString();
void FreeMessageString(std::string *str);

//TODO: re-use objects?
struct asyncVerification{
  asyncVerification(uint32_t _quorumSize, mainThreadCallback mcb, int no_groups,
    proto::CommitDecision _decision, Transport* tp) :  quorumSize(_quorumSize),
    mcb(mcb), groupTotals(no_groups), decision(_decision),
    terminate(false), tp(tp) { }
  ~asyncVerification() { deleteMessages();}

  std::mutex objMutex;
  Transport* tp;
  std::vector<std::string*> ccMsgs;

  void deleteMessages(){
    for(auto ccMsg : ccMsgs){
      FreeMessageString(ccMsg);//delete ccMsg;
    }
  }

  uint32_t quorumSize;
  //std::function<void(bool)> mainThreadCallback;
  mainThreadCallback mcb;

  std::map<uint64_t, uint32_t> groupCounts;
  int groupTotals;
  int groupsVerified = 0;

  proto::CommitDecision decision;
  //proto::Transaction *txn;
  //std::set<int> groupsVerified;

  int deletable;
  bool terminate;
};


template<typename T> static void* pointerWrapper(std::function<T()> func){
    T* t = new T; //(T*) malloc(sizeof(T));
    *t = func();
    return (void*) t;
}

void* BoolPointerWrapper(std::function<bool()> func);

void SignMessage(::google::protobuf::Message* msg,
    crypto::PrivKey* privateKey, uint64_t processId,
    proto::SignedMessage *signedMessage);

void* asyncSignMessage(::google::protobuf::Message* msg,
    crypto::PrivKey* privateKey, uint64_t processId,
    proto::SignedMessage *signedMessage);

void SignMessages(const std::vector<::google::protobuf::Message*>& msgs,
    crypto::PrivKey* privateKey, uint64_t processId,
    const std::vector<proto::SignedMessage*>& signedMessages,
    uint64_t merkleBranchFactor);

    void SignMessages(const std::vector<Triplet>& batch,
        crypto::PrivKey* privateKey, uint64_t processId,
        uint64_t merkleBranchFactor);

void* asyncSignMessages(const std::vector<::google::protobuf::Message*> msgs,
    crypto::PrivKey* privateKey, uint64_t processId,
    const std::vector<proto::SignedMessage*> signedMessages,
    uint64_t merkleBranchFactor);

void asyncValidateCommittedConflict(const proto::CommittedProof &proof,
    const std::string *committedTxnDigest, const proto::Transaction *txn,
    const std::string *txnDigest, bool signedMessages, KeyManager *keyManager,
    const transport::Configuration *config, Verifier *verifier,
    mainThreadCallback mcb, Transport *transport, bool multithread = false, bool batchVerification = false);

void asyncValidateCommittedProof(const proto::CommittedProof &proof,
    const std::string *committedTxnDigest, KeyManager *keyManager,
    const transport::Configuration *config, Verifier *verifier,
    mainThreadCallback mcb, Transport *transport, bool multithread = false, bool batchVerification = false);

bool ValidateCommittedConflict(const proto::CommittedProof &proof,
    const std::string *committedTxnDigest, const proto::Transaction *txn,
    const std::string *txnDigest, bool signedMessages, KeyManager *keyManager,
    const transport::Configuration *config, Verifier *verifier);

bool ValidateCommittedProof(const proto::CommittedProof &proof,
    const std::string *committedTxnDigest, KeyManager *keyManager,
    const transport::Configuration *config, Verifier *verifier);

void* ValidateP1RepliesWrapper(proto::CommitDecision decision,
    bool fast,
    const proto::Transaction *txn,
    const std::string *txnDigest,
    const proto::GroupedSignatures &groupedSigs,
    KeyManager *keyManager,
    const transport::Configuration *config,
    int64_t myProcessId, proto::ConcurrencyControl::Result myResult, Verifier *verifier);

void asyncBatchValidateP1Replies(proto::CommitDecision decision, bool fast, const proto::Transaction *txn,
    const std::string *txnDigest, const proto::GroupedSignatures &groupedSigs, KeyManager *keyManager,
    const transport::Configuration *config, int64_t myProcessId, proto::ConcurrencyControl::Result myResult,
    Verifier *verifier, mainThreadCallback mcb, Transport *transport, bool multithread = false);

void asyncValidateP1Replies(proto::CommitDecision decision, bool fast, const proto::Transaction *txn,
    const std::string *txnDigest, const proto::GroupedSignatures &groupedSigs, KeyManager *keyManager,
    const transport::Configuration *config, int64_t myProcessId, proto::ConcurrencyControl::Result myResult,
    Verifier *verifier, mainThreadCallback mcb, Transport *transport, bool multithread = false);

void asyncValidateP1RepliesCallback(asyncVerification* verifyObj, uint32_t groupId, void* result);
//void ThreadLocalAsyncValidateP1RepliesCallback(asyncVerification* verifyObj, uint32_t groupId, void* result);

bool ValidateP1Replies(proto::CommitDecision decision, bool fast,
    const proto::Transaction *txn,
    const std::string *txnDigest, const proto::GroupedSignatures &groupedSigs,
    KeyManager *keyManager, const transport::Configuration *config,
    int64_t myProcessId, proto::ConcurrencyControl::Result myResult, Verifier *verifier);

bool ValidateP1Replies(proto::CommitDecision decision, bool fast,
    const proto::Transaction *txn,
    const std::string *txnDigest, const proto::GroupedSignatures &groupedSigs,
    KeyManager *keyManager, const transport::Configuration *config,
    int64_t myProcessId, proto::ConcurrencyControl::Result myResult,
    Latency_t &lat, Verifier *verifier);

void* ValidateP2RepliesWrapper(proto::CommitDecision decision, uint64_t view,
    const proto::Transaction *txn,
    const std::string *txnDigest, const proto::GroupedSignatures &groupedSigs,
    KeyManager *keyManager, const transport::Configuration *config,
    int64_t myProcessId, proto::CommitDecision myDecision, Verifier *verifier);

void asyncBatchValidateP2Replies(proto::CommitDecision decision, uint64_t view,
    const proto::Transaction *txn,
    const std::string *txnDigest, const proto::GroupedSignatures &groupedSigs,
    KeyManager *keyManager, const transport::Configuration *config,
    int64_t myProcessId, proto::CommitDecision myDecision, Verifier *verifier,
    mainThreadCallback mcb, Transport* transport, bool multithread = false);

void asyncValidateP2Replies(proto::CommitDecision decision, uint64_t view,
    const proto::Transaction *txn,
    const std::string *txnDigest, const proto::GroupedSignatures &groupedSigs,
    KeyManager *keyManager, const transport::Configuration *config,
    int64_t myProcessId, proto::CommitDecision myDecision, Verifier *verifier,
    mainThreadCallback mcb, Transport* transport, bool multithread = false);

void asyncValidateP2RepliesCallback(asyncVerification* verifyObj, uint32_t groupId, void* result);
//void ThreadLocalAsyncValidateP2RepliesCallback(asyncVerification* verifyObj, uint32_t groupId, void* result);

bool ValidateP2Replies(proto::CommitDecision decision, uint64_t view,
    const proto::Transaction *txn,
    const std::string *txnDigest, const proto::GroupedSignatures &groupedSigs,
    KeyManager *keyManager, const transport::Configuration *config,
    int64_t myProcessId, proto::CommitDecision myDecision, Verifier *verifier);

bool ValidateP2Replies(proto::CommitDecision decision, uint64_t view,
    const proto::Transaction *txn,
    const std::string *txnDigest, const proto::GroupedSignatures &groupedSigs,
    KeyManager *keyManager, const transport::Configuration *config,
    int64_t myProcessId, proto::CommitDecision myDecision,
    Latency_t &lat, Verifier *verifier);

bool ValidateTransactionWrite(const proto::CommittedProof &proof,
    const std::string *txnDigest, const std::string &key, const std::string &val, const Timestamp &timestamp,
    const transport::Configuration *config, bool signedMessages,
    KeyManager *keyManager, Verifier *verifier);

void asyncValidateTransactionWrite(const proto::CommittedProof &proof,
    const std::string *txnDigest,
    const std::string &key, const std::string &val, const Timestamp &timestamp,
    const transport::Configuration *config, bool signedMessages,
    KeyManager *keyManager, Verifier *verifier, mainThreadCallback cb, Transport* transport,
    bool multithread);

// check must validate that proof replies are from all involved shards
bool ValidateProofCommit1(const proto::CommittedProof &proof,
    const std::string &txnDigest,
    const transport::Configuration *config, bool signedMessages,
    KeyManager *keyManager);

bool ValidateProofAbort(const proto::CommittedProof &proof,
    const transport::Configuration *config, bool signedMessages,
    bool hashDigest, KeyManager *keyManager);

bool ValidateP1RepliesCommit(
    const std::map<int, std::vector<proto::Phase1Reply>> &groupedP1Replies,
    const std::string &txnDigest, const proto::Transaction &txn,
    const transport::Configuration *config);

bool ValidateP2RepliesCommit(
    const std::vector<proto::Phase2Reply> &p2Replies,
    const std::string &txnDigest, const proto::Transaction &txn,
    const transport::Configuration *config);

bool ValidateP1RepliesAbort(
    const std::map<int, std::vector<proto::Phase1Reply>> &groupedP1Replies,
    const std::string &txnDigest, const proto::Transaction &txn,
    const transport::Configuration *config, bool signedMessages, bool hashDigest,
    KeyManager *keyManager);

bool ValidateP2RepliesAbort(
    const std::vector<proto::Phase2Reply> &p2Replies,
    const std::string &txnDigest, const proto::Transaction &txn,
    const transport::Configuration *config);


bool ValidateDependency(const proto::Dependency &dep,
    const transport::Configuration *config, uint64_t readDepSize,
    KeyManager *keyManager, Verifier *verifier);

bool operator==(const proto::Write &pw1, const proto::Write &pw2);

bool operator!=(const proto::Write &pw1, const proto::Write &pw2);

std::string TransactionDigest(const proto::Transaction &txn, bool hashDigest);

std::string BytesToHex(const std::string &bytes, size_t maxLength);

bool TransactionsConflict(const proto::Transaction &a,
    const proto::Transaction &b);

uint64_t QuorumSize(const transport::Configuration *config);
uint64_t FastQuorumSize(const transport::Configuration *config);
uint64_t SlowCommitQuorumSize(const transport::Configuration *config);
uint64_t FastAbortQuorumSize(const transport::Configuration *config);
uint64_t SlowAbortQuorumSize(const transport::Configuration *config);
bool IsReplicaInGroup(uint64_t id, uint32_t group,
    const transport::Configuration *config);

int64_t GetLogGroup(const proto::Transaction &txn, const std::string &txnDigest);

enum InjectFailureType {
  CLIENT_EQUIVOCATE = 0,
  CLIENT_CRASH
};

struct InjectFailure {
  InjectFailure() { }
  InjectFailure(const InjectFailure &failure) : type(failure.type),
      timeMs(failure.timeMs), enabled(failure.enabled), frequency(failure.frequency) { }

  InjectFailureType type;
  uint32_t timeMs;
  bool enabled;
  uint32_t frequency;
};

typedef struct Parameters {
  const bool signedMessages;
  const bool validateProofs;
  const bool hashDigest;
  const bool verifyDeps;
  const int signatureBatchSize;
  const int64_t maxDepDepth;
  const uint64_t readDepSize;
  const bool readReplyBatch;
  const bool adjustBatchSize;
  const bool sharedMemBatches;
  const bool sharedMemVerify;
  const uint64_t merkleBranchFactor;
  const InjectFailure injectFailure;

  const bool multiThreading;
  const bool batchVerification;
  const int verificationBatchSize;

  const bool mainThreadDispatching;
  const bool dispatchMessageReceive;
  const bool parallel_reads;
  const bool parallel_CCC;
  const bool dispatchCallbacks;

  Parameters(bool signedMessages, bool validateProofs, bool hashDigest, bool verifyDeps,
    int signatureBatchSize, int64_t maxDepDepth, uint64_t readDepSize,
    bool readReplyBatch, bool adjustBatchSize, bool sharedMemBatches,
    bool sharedMemVerify, uint64_t merkleBranchFactor, const InjectFailure &injectFailure,
    bool multiThreading, bool batchVerification, int verificationBatchSize,
    bool mainThreadDispatching, bool dispatchMessageReceive,
    bool parallel_reads,
    bool parallel_CCC,
    bool dispatchCallbacks) :
    signedMessages(signedMessages), validateProofs(validateProofs),
    hashDigest(hashDigest), verifyDeps(verifyDeps), signatureBatchSize(signatureBatchSize),
    maxDepDepth(maxDepDepth), readDepSize(readDepSize),
    readReplyBatch(readReplyBatch), adjustBatchSize(adjustBatchSize),
    sharedMemBatches(sharedMemBatches), sharedMemVerify(sharedMemVerify),
    merkleBranchFactor(merkleBranchFactor), injectFailure(injectFailure),
    multiThreading(multiThreading), batchVerification(batchVerification),
    verificationBatchSize(verificationBatchSize),
    mainThreadDispatching(mainThreadDispatching),
    dispatchMessageReceive(dispatchMessageReceive),
    parallel_reads(parallel_reads),
    parallel_CCC(parallel_CCC),
    dispatchCallbacks(dispatchCallbacks) { }
} Parameters;

} // namespace indicusstore

#endif /* INDICUS_COMMON_H */
