// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * store/indicusstore/server.cc:
 *   Implementation of a single transactional key-value server.
 *
 * Copyright 2015 Irene Zhang <iyzhang@cs.washington.edu>
 *                Naveen Kr. Sharma <naveenks@cs.washington.edu>
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


#include "store/indicusstore/server.h"

#include <bitset>
#include <queue>
#include <ctime>
#include <chrono>
#include <sys/time.h>

#include "lib/assert.h"
#include "lib/tcptransport.h"
#include "store/indicusstore/common.h"
#include "store/indicusstore/phase1validator.h"
#include "store/indicusstore/localbatchsigner.h"
#include "store/indicusstore/sharedbatchsigner.h"
#include "store/indicusstore/basicverifier.h"
#include "store/indicusstore/localbatchverifier.h"
#include "store/indicusstore/sharedbatchverifier.h"
#include "lib/batched_sigs.h"
#include <valgrind/memcheck.h>

namespace indicusstore {

Server::Server(const transport::Configuration &config, int groupIdx, int idx,
    int numShards, int numGroups, Transport *transport, KeyManager *keyManager,
    Parameters params, uint64_t timeDelta, OCCType occType, Partitioner *part,
    unsigned int batchTimeoutMicro, TrueTime timeServer) :
    PingServer(transport),
    config(config), groupIdx(groupIdx), idx(idx), numShards(numShards),
    numGroups(numGroups), id(groupIdx * config.n + idx),
    transport(transport), occType(occType), part(part),
    params(params), keyManager(keyManager),
    timeDelta(timeDelta),
    timeServer(timeServer)
     {



  store.KVStore_Reserve(4200000);
  //Used for Fallback all to all:
  CreateSessionKeys();
  //////

  Debug("Starting Indicus replica %d.", id);
  transport->Register(this, config, groupIdx, idx);
  _Latency_Init(&committedReadInsertLat, "committed_read_insert_lat");
  _Latency_Init(&verifyLat, "verify_lat");
  _Latency_Init(&signLat, "sign_lat");
  _Latency_Init(&waitingOnLocks, "lock_lat");
  //_Latency_Init(&waitOnProtoLock, "proto_lock_lat");
  //_Latency_Init(&store.storeLockLat, "store_lock_lat");

  if (params.signatureBatchSize == 1) {
    //verifier = new BasicVerifier(transport);
    verifier = new BasicVerifier(transport, batchTimeoutMicro, params.validateProofs &&
      params.signedMessages && params.adjustBatchSize, params.verificationBatchSize);
    batchSigner = nullptr;
  } else {
    if (params.sharedMemBatches) {
      batchSigner = new SharedBatchSigner(transport, keyManager, GetStats(),
          batchTimeoutMicro, params.signatureBatchSize, id,
          params.validateProofs && params.signedMessages &&
          params.signatureBatchSize > 1 && params.adjustBatchSize,
          params.merkleBranchFactor);
    } else {
      batchSigner = new LocalBatchSigner(transport, keyManager, GetStats(),
          batchTimeoutMicro, params.signatureBatchSize, id,
          params.validateProofs && params.signedMessages &&
          params.signatureBatchSize > 1 && params.adjustBatchSize,
          params.merkleBranchFactor);
    }

    if (params.sharedMemVerify) {
      verifier = new SharedBatchVerifier(params.merkleBranchFactor, stats); //add transport if using multithreading
    } else {
      //verifier = new LocalBatchVerifier(params.merkleBranchFactor, stats, transport);
      verifier = new LocalBatchVerifier(params.merkleBranchFactor, stats, transport,
        batchTimeoutMicro, params.validateProofs && params.signedMessages &&
        params.signatureBatchSize > 1 && params.adjustBatchSize, params.verificationBatchSize);
    }
  }
  //start up threadpool (common threadpool, not split into different roles yet)
  //tp = new ThreadPool();

  // this is needed purely from loading data without executing transactions
  proto::CommittedProof *proof = new proto::CommittedProof();
  proof->mutable_txn()->set_client_id(0);
  proof->mutable_txn()->set_client_seq_num(0);
  proof->mutable_txn()->mutable_timestamp()->set_timestamp(0);
  proof->mutable_txn()->mutable_timestamp()->set_id(0);
   //if(params.mainThreadDispatching) committedMutex.lock();
  committed.insert(std::make_pair("", proof));
   //if(params.mainThreadDispatching) committedMutex.unlock();
}

Server::~Server() {
  std::cerr << "KVStore size: " << store.KVStore_size() << std::endl;
  std::cerr << "ReadStore size: " << store.ReadStore_size() << std::endl;
  std::cerr << "commitGet count: " << commitGet_count << std::endl;

  std::cerr << "Hash count: " << BatchedSigs::hashCount << std::endl;
  std::cerr << "Hash cat count: " << BatchedSigs::hashCatCount << std::endl;
  std::cerr << "Total count: " << BatchedSigs::hashCount + BatchedSigs::hashCatCount << std::endl;

  std::cerr << "Store wait latency (ms): " << store.lock_time << std::endl;
  Latency_Dump(&waitingOnLocks);
  //Latency_Dump(&waitOnProtoLock);
  //Latency_Dump(&batchSigner->waitOnBatchLock);
  //Latency_Dump(&(store.storeLockLat));
  Notice("Freeing verifier.");
  delete verifier;
   //if(params.mainThreadDispatching) committedMutex.lock();
  for (const auto &c : committed) {
    delete c.second;
  }
   //if(params.mainThreadDispatching) committedMutex.unlock();
   ////if(params.mainThreadDispatching) ongoingMutex.lock();
  for (const auto &o : ongoing) {
    delete o.second;
  }
   ////if(params.mainThreadDispatching) ongoingMutex.unlock();
   if(params.mainThreadDispatching) readReplyProtoMutex.lock();
  for (auto r : readReplies) {
    delete r;
  }
   if(params.mainThreadDispatching) readReplyProtoMutex.unlock();
   if(params.mainThreadDispatching) p1ReplyProtoMutex.lock();
  for (auto r : p1Replies) {
    delete r;
  }
   if(params.mainThreadDispatching) p1ReplyProtoMutex.unlock();
   if(params.mainThreadDispatching) p2ReplyProtoMutex.lock();
  for (auto r : p2Replies) {
    delete r;
  }
   if(params.mainThreadDispatching) p2ReplyProtoMutex.unlock();
  Notice("Freeing signer.");
  if (batchSigner != nullptr) {
    delete batchSigner;
  }
  Latency_Dump(&verifyLat);
  Latency_Dump(&signLat);

}

 void PrintSendCount(){
   send_count++;
   fprintf(stderr, "send count: %d \n", send_count);
 }

 void PrintRcvCount(){
   rcv_count++;
   fprintf(stderr, "rcv count: %d\n", rcv_count);
 }

 void ParseProto(::google::protobuf::Message *msg, std::string &data){
   msg->ParseFromString(data);
 }
// use like this: main dispatches lambda
// auto f = [this, msg, type, data](){
//   ParseProto;
//   [this, msg, type](){
//     this->HandleType(msg, type);
//   };
//   this->transport->DispatchTP_main(g);
//   return (void*) true;
// };
// DispatchTP_noCB(f);
// Main dispatches serialization, and lets serialization thread dispatch to main2


 void Server::ReceiveMessage(const TransportAddress &remote,
       const std::string &type, const std::string &data, void *meta_data) {

  // debug_counters[remote]++;
  // //fprintf(stderr, "All message receptions go through CPU %d \n", sched_getcpu());
  // fprintf(stderr, "Message received [%d][]%d]", debug_counters[remote]); //print transport address.
  if(params.dispatchMessageReceive){
    Debug("Dispatching message handling to Support Main Thread");
   transport->DispatchTP_main([this, &remote, type, data, meta_data]() { //ends up with more copies here..
     this->ReceiveMessageInternal(remote, type, data, meta_data);
     return (void*) true;
   });
  }
  else{
    ReceiveMessageInternal(remote, type, data, meta_data);
  }
 }
//Full CPU utilization parallelism: Assign all these functions to different threads. Add mutexes to every shared data structure function
void Server::ReceiveMessageInternal(const TransportAddress &remote,
      const std::string &type, const std::string &data, void *meta_data) {


  //std::unique_lock<std::mutex> main_lock(mainThreadMutex);
  ////auto lockScope = params.mainThreadDispatching ? std::unique_lock<std::mutex>(mainThreadMutex) : std::unique_lock<std::mutex>();

  if (type == read.GetTypeName()) {

    if(!params.mainThreadDispatching || params.dispatchMessageReceive){
      read.ParseFromString(data);
      HandleRead(remote, read);
    }
    else{
      proto::Read* readCopy = GetUnusedReadmessage();
      readCopy->ParseFromString(data);
      auto f = [this, &remote, readCopy](){
        //std::unique_lock<std::mutex> main_lock(this->mainThreadMutex);
        this->HandleRead(remote, *readCopy);
        //FreeReadmessage(readCopy); //TODO: can do in callback of reply?
        return (void*) true;
      };
      if(params.parallel_reads){
        transport->DispatchTP_noCB(f);
      }
      else{
        transport->DispatchTP_main(f);
      }


    }
  } else if (type == phase1.GetTypeName()) {

    if(!params.mainThreadDispatching || params.dispatchMessageReceive){
     phase1.ParseFromString(data);
     HandlePhase1(remote, phase1);
    }
    else{
      proto::Phase1 *phase1Copy = GetUnusedPhase1message();
      phase1Copy->ParseFromString(data);
      auto f = [this, &remote, phase1Copy]() {  //mutable keyword to change const into none
        //std::unique_lock<std::mutex> main_lock(this->mainThreadMutex);
        this->HandlePhase1(remote, *phase1Copy);
        //FreePhase1message(phase1Copy); //TODO: Can do before sendphase1reply?
        return (void*) true;
      };
      Debug("Dispatching HandlePhase1");
      transport->DispatchTP_main(f);
      //transport->DispatchTP_noCB(f);
    }

  } else if (type == phase2.GetTypeName()) {

      if(params.multiThreading){
          proto::Phase2* p2 = GetUnusedPhase2message();
          p2->ParseFromString(data);
          if(!params.mainThreadDispatching){
            HandlePhase2(remote, *p2);
          }
          else{
            auto f = [this, &remote, p2](){
              //std::unique_lock<std::mutex> main_lock(this->mainThreadMutex);
              this->HandlePhase2(remote, *p2);
              return (void*) true;
            };
            transport->DispatchTP_main(f);
          }
      }
      else{

        if(!params.mainThreadDispatching || params.dispatchMessageReceive){
          phase2.ParseFromString(data);
          HandlePhase2(remote, phase2);
        }
        else{
          proto::Phase2* p2 = GetUnusedPhase2message();
          p2->ParseFromString(data);
          auto f = [this, &remote, p2](){
            //std::unique_lock<std::mutex> main_lock(this->mainThreadMutex);
            this->HandlePhase2(remote, *p2);
            return (void*) true;
          };
          transport->DispatchTP_main(f);
        }
      }

  } else if (type == writeback.GetTypeName()) {
      if(params.multiThreading){
          proto::Writeback *wb = GetUnusedWBmessage();
          wb->ParseFromString(data);
          if(!params.mainThreadDispatching){
            HandleWriteback(remote, *wb);
          }
          else{
            auto f = [this, &remote, wb](){
              //std::unique_lock<std::mutex> main_lock(this->mainThreadMutex);
              this->HandleWriteback(remote, *wb);
              return (void*) true;
            };
            transport->DispatchTP_main(f);
          }
      }
      else{

        //HandleWriteback(remote, writeback);
        if(!params.mainThreadDispatching || params.dispatchMessageReceive){
          writeback.ParseFromString(data);
          HandleWriteback(remote, writeback);
        }
        else{
          proto::Writeback *wb = GetUnusedWBmessage();
          wb->ParseFromString(data);
          auto f = [this, &remote, wb](){
            //std::unique_lock<std::mutex> main_lock(this->mainThreadMutex);
            this->HandleWriteback(remote, *wb);
            return (void*) true;
          };
          transport->DispatchTP_main(f);
        }

      }
  } else if (type == abort.GetTypeName()) {
    abort.ParseFromString(data);
    HandleAbort(remote, abort);
  } else if (type == ping.GetTypeName()) {
    ping.ParseFromString(data);
    Debug("Ping is called");
    HandlePingMessage(this, remote, ping);

// Add all Fallback signedMessages
  } else if (type == phase1FB.GetTypeName()) {
    phase1FB.ParseFromString(data);
    HandlePhase1FB(remote, phase1FB);
  } else if (type == phase2FB.GetTypeName()) {
    phase2FB.ParseFromString(data);
    HandlePhase2FB(remote, phase2FB);
  } else if (type == invokeFB.GetTypeName()) {
    invokeFB.ParseFromString(data);
    HandleInvokeFB(remote, invokeFB); //DONT send back to remote, but instead to FB, calculate based on view. (need to include this in TX state thats kept locally.)
  } else if (type == electFB.GetTypeName()) {
    electFB.ParseFromString(data);
    HandleElectFB(remote, electFB);
  } else if (type == decisionFB.GetTypeName()) {
    decisionFB.ParseFromString(data);
    HandleDecisionFB(remote, decisionFB); //DONT send back to remote, but instead to interested clients. (need to include list of interested clients as part of local tx state)
  //} else if (type == moveView.GetTypeName()) {
  //  moveView.ParseFromString(data);
  //  HandleMoveView(remote, moveView); //Send only to other replicas
  } else {
    Panic("Received unexpected message type: %s", type.c_str());
  }
}

//Debug("Stuck at line %d", __LINE__);
void Server::Load(const std::string &key, const std::string &value,
    const Timestamp timestamp) {
  Value val;
  val.val = value;
   //if(params.mainThreadDispatching) committedMutex.lock_shared();
  auto committedItr = committed.find("");
  UW_ASSERT(committedItr != committed.end());
  val.proof = committedItr->second;
   //if(params.mainThreadDispatching) committedMutex.unlock_shared();
   //if(params.mainThreadDispatching) storeMutex.lock();
  store.put(key, val, timestamp);
   //if(params.mainThreadDispatching) storeMutex.unlock();
  if (key.length() == 5 && key[0] == 0) {
    std::cerr << std::bitset<8>(key[0]) << ' '
              << std::bitset<8>(key[1]) << ' '
              << std::bitset<8>(key[2]) << ' '
              << std::bitset<8>(key[3]) << ' '
              << std::bitset<8>(key[4]) << ' '
              << std::endl;
  }
}

void Server::HandleRead(const TransportAddress &remote,
     proto::Read &msg) {

  //auto lockScope = params.mainThreadDispatching ? std::unique_lock<std::mutex>(mainThreadMutex) : std::unique_lock<std::mutex>();
  Debug("READ[%lu:%lu] for key %s with ts %lu.%lu.", msg.timestamp().id(),
      msg.req_id(), BytesToHex(msg.key(), 16).c_str(),
      msg.timestamp().timestamp(), msg.timestamp().id());
  Timestamp ts(msg.timestamp());
  if (CheckHighWatermark(ts)) {
    // ignore request if beyond high watermark
    Debug("Read timestamp beyond high watermark.");
    return;
  }

  std::pair<Timestamp, Server::Value> tsVal;
   //if(params.mainThreadDispatching) storeMutex.lock();
  bool exists = store.get(msg.key(), ts, tsVal);
   //if(params.mainThreadDispatching) storeMutex.unlock();

  proto::ReadReply* readReply = GetUnusedReadReply();
  readReply->set_req_id(msg.req_id());
  readReply->set_key(msg.key());
  if (exists) {
    Debug("READ[%lu] Committed value of length %lu bytes with ts %lu.%lu.",
        msg.req_id(), tsVal.second.val.length(), tsVal.first.getTimestamp(),
        tsVal.first.getID());
    readReply->mutable_write()->set_committed_value(tsVal.second.val);
    tsVal.first.serialize(readReply->mutable_write()->mutable_committed_timestamp());
    if (params.validateProofs) {
      *readReply->mutable_proof() = *tsVal.second.proof;
    }
  }

  TransportAddress *remoteCopy = remote.clone();

  auto sendCB = [this, remoteCopy, readReply]() {
    //std::unique_lock<std::mutex> lock(transportMutex);

    this->transport->SendMessage(this, *remoteCopy, *readReply);
    delete remoteCopy;
    FreeReadReply(readReply);
  };
  //TESTING:
  // auto sendCB_wrapped = [this, remoteCopy, readReply](void* arg) {
  //   //std::unique_lock<std::mutex> lock(transportMutex);
  //
  //   this->transport->SendMessage(this, *remoteCopy, *readReply);
  //   delete remoteCopy;
  //   FreeReadReply(readReply);
  // };
  //
  // auto sendCB = [this, scb = std::move(sendCB_wrapped)]() {
  //   //std::unique_lock<std::mutex> lock(transportMutex);
  //   this->transport->IssueCB(scb, (void*) true);
  // };

  if (occType == MVTSO) {
    /* update rts */
    // TODO: how to track RTS by transaction without knowing transaction digest?


    //RECOMMENT
    //  if(params.mainThreadDispatching) rtsMutex.lock();
    // rts[msg.key()].insert(ts);
    //  if(params.mainThreadDispatching) rtsMutex.unlock();
     //Testing:
     auto itr = rts.find(msg.key());
     if(itr != rts.end()){
       if(ts.getTimestamp() > itr->second ) {
         rts[msg.key()] = ts.getTimestamp();
       }
     }
     else{
       rts[msg.key()] = ts.getTimestamp();
     }


    /* add prepared deps */
    if (params.maxDepDepth > -2) {
      const proto::Transaction *mostRecent = nullptr;


      //auto preparedWritesMutexScope = params.mainThreadDispatching ? std::shared_lock<std::shared_mutex>(preparedWritesMutex) : std::shared_lock<std::shared_mutex>();

      auto itr = preparedWrites.find(msg.key());
      if (itr != preparedWrites.end()){

        //std::pair &x = preparedWrites[write.key()];
        std::shared_lock lock(itr->second.first);
        //if(itr->second.second.size() > 0) {

        // there is a prepared write for the key being read

        for (const auto &t : itr->second.second) {
          if (mostRecent == nullptr || t.first > Timestamp(mostRecent->timestamp())) { //TODO: only use it if its bigger than the committed write..
            mostRecent = t.second;
          }
        }

        if (mostRecent != nullptr) {
          std::string preparedValue;
          for (const auto &w : mostRecent->write_set()) {
            if (w.key() == msg.key()) {
              preparedValue = w.value();
              break;
            }
          }

          Debug("Prepared write with most recent ts %lu.%lu.",
              mostRecent->timestamp().timestamp(), mostRecent->timestamp().id());

          if (params.maxDepDepth == -1 || DependencyDepth(mostRecent) <= params.maxDepDepth) {
            readReply->mutable_write()->set_prepared_value(preparedValue);
            *readReply->mutable_write()->mutable_prepared_timestamp() = mostRecent->timestamp();
            *readReply->mutable_write()->mutable_prepared_txn_digest() = TransactionDigest(*mostRecent, params.hashDigest);
          }

        }
      // }
      }
    }
  }


  if (params.validateProofs && params.signedMessages &&
      (readReply->write().has_committed_value() || (params.verifyDeps && readReply->write().has_prepared_value()))) {

//If readReplyBatch is false then respond immediately, otherwise respect batching policy
    if (params.readReplyBatch) {
      proto::Write* write = new proto::Write(readReply->write());
      // move: sendCB = std::move(sendCB) or {std::move(sendCB)}
      MessageToSign(write, readReply->mutable_signed_write(), [sendCB, write]() {
        sendCB();
        delete write;
      });

    } else if (params.signatureBatchSize == 1) {

      if(params.multiThreading){
        proto::Write* write = new proto::Write(readReply->write());
        // std::function<void*()> f(std::bind(asyncSignMessage, write,
        //   keyManager->GetPrivateKey(id), id, readReply->mutable_signed_write()));
        // transport->DispatchTP(std::move(f), [sendCB, write](void * ret){ sendCB(); delete write;});

        auto f = [this, readReply, sendCB = std::move(sendCB), write]()
        {
          SignMessage(write, keyManager->GetPrivateKey(id), id, readReply->mutable_signed_write());
          sendCB();
          delete write;
          return (void*) true;
        };
        transport->DispatchTP_noCB(std::move(f));


      }
      else{
        proto::Write write(readReply->write());
        SignMessage(&write, keyManager->GetPrivateKey(id), id,
            readReply->mutable_signed_write());
        sendCB();
      }

    } else {

      if(params.multiThreading){

        std::vector<::google::protobuf::Message *> msgs;
        proto::Write* write = new proto::Write(readReply->write()); //TODO might want to re-use
        msgs.push_back(write);
        std::vector<proto::SignedMessage *> smsgs;
        smsgs.push_back(readReply->mutable_signed_write());

        auto f = [this, msgs, smsgs, sendCB = std::move(sendCB), write]()
        {
          SignMessages(msgs, keyManager->GetPrivateKey(id), id, smsgs, params.merkleBranchFactor);
          sendCB();
          delete write;
          return (void*) true;
        };
        transport->DispatchTP_noCB(std::move(f));


        //TESTING
        // auto g = [this, msgs, smsgs, sendCB = std::move(sendCB), write](void* arg)
        // {
        //   SignMessages(msgs, keyManager->GetPrivateKey(id), id, smsgs, params.merkleBranchFactor);
        //   sendCB();
        //   delete write;
        //   return (void*) true;
        // };
        // transport->IssueCB(std::move(g), (void*) true);

        ////

        // std::function<void*()> f(std::bind(asyncSignMessages, msgs,
        //   keyManager->GetPrivateKey(id), id, smsgs, params.merkleBranchFactor));
        // transport->DispatchTP(std::move(f) ,[sendCB, write](void * ret){ sendCB(); delete write;});
      }
      else{
        proto::Write write(readReply->write());
        std::vector<::google::protobuf::Message *> msgs;
        msgs.push_back(&write);
        std::vector<proto::SignedMessage *> smsgs;
        smsgs.push_back(readReply->mutable_signed_write());
        SignMessages(msgs, keyManager->GetPrivateKey(id), id, smsgs, params.merkleBranchFactor);
        sendCB();
      }
    }
  } else {
    sendCB();
  }
  if(params.mainThreadDispatching && !params.dispatchMessageReceive) FreeReadmessage(&msg);
}

//////////////////////

void Server::HandlePhase1(const TransportAddress &remote,
    proto::Phase1 &msg) {
//auto lockScope = params.mainThreadDispatching ? std::unique_lock<std::mutex>(mainThreadMutex) : std::unique_lock<std::mutex>();

  std::string txnDigest = TransactionDigest(msg.txn(), params.hashDigest);
  Debug("PHASE1[%lu:%lu][%s] with ts %lu.", msg.txn().client_id(),
      msg.txn().client_seq_num(), BytesToHex(txnDigest, 16).c_str(),
      msg.txn().timestamp().timestamp());
  proto::ConcurrencyControl::Result result;
  const proto::CommittedProof *committedProof;
  // no-replays property, i.e. recover existing decision/result from storage
  //Latency_Start(&waitingOnLocks);
   //if(params.mainThreadDispatching) p1ConflictsMutex.lock();
   //if(params.mainThreadDispatching) p1DecisionsMutex.lock();
   //if(params.mainThreadDispatching) interestedClientsMutex.lock();
  //Latency_End(&waitingOnLocks);

  p1DecisionsMap::const_accessor c;
  bool p1DecItr = p1Decisions.find(c, txnDigest);
  if(p1DecItr){  //TODO: only do Phase1 if one has not committed/aborted already.
  //if(p1Decisions.find(txnDigest) != p1Decisions.end()){
    //result = p1Decisions[txnDigest];
    result = c->second;
    c.release();
    //KEEP track of interested client

    interestedClientsMap::accessor i;
    bool interestedClientsItr = interestedClients.insert(i, txnDigest);
    i->second.insert(remote.clone());
    i.release();
    //interestedClients[txnDigest].insert(remote.clone());

    if (result == proto::ConcurrencyControl::ABORT) {
      p1ConflictsMap::const_accessor d;
      auto p1ConflictsItr = p1Conflicts.find(d, txnDigest);
      //committedProof = p1Conflicts[txnDigest];
      committedProof = d->second;
      d.release();

    }
     //if(params.mainThreadDispatching) p1ConflictsMutex.unlock();
     //if(params.mainThreadDispatching) p1DecisionsMutex.unlock();
     //if(params.mainThreadDispatching) interestedClientsMutex.unlock();
  } else{
    c.release();
     //if(params.mainThreadDispatching) p1ConflictsMutex.unlock();
     //if(params.mainThreadDispatching) p1DecisionsMutex.unlock();
     //if(params.mainThreadDispatching) interestedClientsMutex.unlock();
    if (params.validateProofs && params.signedMessages && params.verifyDeps) {
      for (const auto &dep : msg.txn().deps()) {
        if (!dep.has_write_sigs()) {
          Debug("Dep for txn %s missing signatures.",
              BytesToHex(txnDigest, 16).c_str());
          return;
        }
        if (!ValidateDependency(dep, &config, params.readDepSize, keyManager,
              verifier)) {
          Debug("VALIDATE Dependency failed for txn %s.",
              BytesToHex(txnDigest, 16).c_str());
          // safe to ignore Byzantine client
          return;
        }
      }
    }
    //KEEP track of interested client
    //Latency_Start(&waitingOnLocks);
     //if(params.mainThreadDispatching) interestedClientsMutex.lock();
     if(params.mainThreadDispatching) current_viewsMutex.lock();
    //Latency_End(&waitingOnLocks);
    current_views[txnDigest] = 0;
      std::cout << "ABORTING Before clients" << std::endl;
      interestedClientsMap::accessor i;
      bool interestedClientsItr = interestedClients.insert(i, txnDigest);
      i->second.insert(remote.clone());
      i.release();
      //interestedClients[txnDigest].insert(remote.clone());
      std::cout << "ABORTING AFTEr clients" << std::endl;
     //if(params.mainThreadDispatching) interestedClientsMutex.unlock();
     if(params.mainThreadDispatching) current_viewsMutex.unlock();

    proto::Transaction *txn = msg.release_txn();

     //if(params.mainThreadDispatching) ongoingMutex.lock();
     ongoingMap::accessor b;
     ongoing.insert(b, std::make_pair(txnDigest, txn));
     b.release();
     //ongoing[txnDigest] = txn;
     //if(params.mainThreadDispatching) ongoingMutex.unlock();

    Timestamp retryTs;

    if(true){
      result = DoOCCCheck(msg.req_id(), remote, txnDigest, *txn, retryTs,
          committedProof);
    }
    else{
      // auto cb = [this, msg_ptr = &msg, remote_ptr = &remote, txnDigest, txn,  committedProof](void* result) mutable{
      //   this->HandlePhase1CB(msg_ptr, *((proto::ConcurrencyControl::Result *) result), committedProof, txnDigest, *remote_ptr);
      // };

      //std::cerr << "TxnDigest: " << BytesToHex(txnDigest, 16) << std::endl;
      auto f = [this, msg_ptr = &msg, remote_ptr = &remote, txnDigest, txn, committedProof]() mutable {
        Timestamp retryTs;

        {
          std::shared_lock<std::shared_mutex> lock(ongoingMutex);
          ongoingMap::const_accessor b;
          if(!ongoing.find(b, txnDigest)){
          //if(ongoing.find(txnDigest) == ongoing.end()){
            Debug("Already concurrently Committed/Aborted txn[%s]", BytesToHex(txnDigest, 16).c_str());
            return (void*) false;
          }
          b.release();
        }

        proto::ConcurrencyControl::Result *result = new proto::ConcurrencyControl::Result(this->DoOCCCheck(msg_ptr->req_id(),
        *remote_ptr, txnDigest, *txn, retryTs, committedProof));

        HandlePhase1CB(msg_ptr, *result, committedProof, txnDigest, *remote_ptr);
        delete result;
        return (void*) true;
      };
      transport->DispatchTP_noCB(std::move(f));
      //transport->DispatchTP(std::move(f), std::move(cb));
      return;
    }
  }

  HandlePhase1CB(&msg, result, committedProof, txnDigest, remote);

  // if (result != proto::ConcurrencyControl::WAIT) {
  //   // if(client_starttime.find(txnDigest) == client_starttime.end()){
  //   //   struct timeval tv;
  //   //   gettimeofday(&tv, NULL);
  //   //   uint64_t start_time = (tv.tv_sec*1000000+tv.tv_usec)/1000;  //in miliseconds
  //   //   client_starttime[txnDigest] = start_time;
  //   // }//time(NULL); //TECHNICALLY THIS SHOULD ONLY START FOR THE ORIGINAL CLIENT, i.e. if another client manages to do it first it shouldnt count... Then again, that client must have gotten it somewhere, so the timer technically started.
  //   SendPhase1Reply(msg.req_id(), result, committedProof, txnDigest, &remote);
  // }
  // if(params.mainThreadDispatching && !params.dispatchMessageReceive) FreePhase1message(&msg);

}

//TODO: move p1Decision into this function (not sendp1: Then, can unlock here.)
void Server::HandlePhase1CB(proto::Phase1 *msg, proto::ConcurrencyControl::Result result,
  const proto::CommittedProof* &committedProof, std::string &txnDigest, const TransportAddress &remote){
  if (result != proto::ConcurrencyControl::WAIT) {
    // if(client_starttime.find(txnDigest) == client_starttime.end()){
    //   struct timeval tv;
    //   gettimeofday(&tv, NULL);
    //   uint64_t start_time = (tv.tv_sec*1000000+tv.tv_usec)/1000;  //in miliseconds
    //   client_starttime[txnDigest] = start_time;
    // }//time(NULL); //TECHNICALLY THIS SHOULD ONLY START FOR THE ORIGINAL CLIENT, i.e. if another client manages to do it first it shouldnt count... Then again, that client must have gotten it somewhere, so the timer technically started.
    SendPhase1Reply(msg->req_id(), result, committedProof, txnDigest, &remote);
  }
  if(params.mainThreadDispatching && !params.dispatchMessageReceive) FreePhase1message(msg);
}

   //TODO: Needs to always delete/free. But does not need the bool arg to be allocated.
void Server::HandlePhase2CB(proto::Phase2 *msg, const std::string* txnDigest,
  signedCallback sendCB, proto::Phase2Reply* phase2Reply, cleanCallback cleanCB, void* valid){ //bool valid) { //

  //auto lockScope = params.mainThreadDispatching ? std::unique_lock<std::mutex>(mainThreadMutex) : std::unique_lock<std::mutex>();
  Debug("HandlePhase2CB invoked");

  auto f = [this, msg, txnDigest, sendCB = std::move(sendCB), phase2Reply, cleanCB = std::move(cleanCB), valid ](){
  if(!valid){
    Debug("VALIDATE P1Replies for TX %s failed.", BytesToHex(*txnDigest, 16).c_str());
    //delete (bool*) valid;
    cleanCB(); //deletes SendCB resources should it not be called.
    if(params.multiThreading || params.mainThreadDispatching){
      FreePhase2message(msg); //const_cast<proto::Phase2&>(msg));
    }
    return (void*) true;
  }

   if(params.mainThreadDispatching) p2DecisionsMutex.lock();
   if(params.mainThreadDispatching) current_viewsMutex.lock();
   if(params.mainThreadDispatching) decision_viewsMutex.lock();

  if(p2Decisions.find(msg->txn_digest()) != p2Decisions.end()){
    proto::CommitDecision &decision =p2Decisions[msg->txn_digest()];
    phase2Reply->mutable_p2_decision()->set_decision(decision);
  }
  else{
    p2Decisions[*txnDigest] = msg->decision();
    current_views[*txnDigest] = 0;
    decision_views[*txnDigest] = 0;
    phase2Reply->mutable_p2_decision()->set_decision(msg->decision());
  }

 if(params.mainThreadDispatching) p2DecisionsMutex.unlock();
 if(params.mainThreadDispatching) current_viewsMutex.unlock();

  // if(client_starttime.find(*txnDigest) == client_starttime.end()){
  //   struct timeval tv;
  //   gettimeofday(&tv, NULL);
  //   uint64_t start_time = (tv.tv_sec*1000000+tv.tv_usec)/1000;  //in miliseconds
  //   client_starttime[*txnDigest] = start_time;
  // }


  if (params.validateProofs) {
    // TODO: need a way for a process to know the decision view when verifying the signed p2_decision
    phase2Reply->mutable_p2_decision()->set_view(decision_views[*txnDigest]);
  }

 if(params.mainThreadDispatching) decision_viewsMutex.unlock();

//Free allocated memory
//delete (bool*) valid;
if(params.multiThreading || params.mainThreadDispatching){
  FreePhase2message(msg); // const_cast<proto::Phase2&>(msg));
}

if (params.validateProofs && params.signedMessages) {
  proto::Phase2Decision* p2Decision = new proto::Phase2Decision(phase2Reply->p2_decision());
  //Latency_Start(&signLat);
  // if(params.mainThreadDispatching) lockScope.unlock();
  MessageToSign(p2Decision, phase2Reply->mutable_signed_p2_decision(),
      [sendCB, p2Decision]() {
      sendCB();
      delete p2Decision;
      });
  //Latency_End(&signLat);
  return (void*) true;
}
sendCB();

return (void*) true;
};
if(params.multiThreading && params.mainThreadDispatching && params.dispatchCallbacks){
  transport->DispatchTP_main(std::move(f));
}
else{
  f();
}

}


//TODO: ADD AUTHENTICATION IN ORDER TO ENFORCE TIMEOUTS. ANYBODY THAT IS NOT THE ORIGINAL CLIENT SHOULD ONLY BE ABLE TO SEND P2FB messages and NOT normal P2!!!!!!
// //TODO: client signatures need to be implemented. keymanager would need to associate with client ids.
// Perhaps client ids can just start after all replica ids, then one can use the keys.
//     if(params.clientAuthenticated){ //TODO: this branch needs to go around the validate Proofs.
//       if(!msg.has_auth_p2dec()){
//         Debug("PHASE2 message not authenticated");
//         return;
//       }
//       proto::Phase2ClientDecision p2cd;
//       p2cd.ParseFromString(msg.auth_p2dec().data());
//       //TODO: the parsing of txn and txn_digest
//
//       if(msg.auth_p2dec().process_id() != txn.client_id()){
//         Debug("PHASE2 message from unauthenticated client");
//         return;
//       }
//       //Todo: Get client key... Currently unimplemented. Clients not authenticated.
//       crypto::Verify(clientPublicKey, &msg.auth_p2dec().data()[0], msg.auth_p2dec().data().length(), &msg.auth_p2dec().signature()[0]);
//
//
//       //TODO: 1) check that message is signed
//       //TODO: 2) check if id matches txn id. Access
//       //TODO: 3) check whether signature validates.
//     }
//     else{
//
//     }

void Server::HandlePhase2(const TransportAddress &remote,
       proto::Phase2 &msg) {
  //auto lockScope = params.mainThreadDispatching ? std::unique_lock<std::mutex>(mainThreadMutex) : std::unique_lock<std::mutex>();

  const proto::Transaction *txn;
  std::string computedTxnDigest;
  const std::string *txnDigest = &computedTxnDigest;
  if (params.validateProofs) {
    if (!msg.has_txn() && !msg.has_txn_digest()) {
      Debug("PHASE2 message contains neither txn nor txn_digest.");
      return;
    }

    if (msg.has_txn_digest()) {
      //auto ongoingMutexScope = params.mainThreadDispatching ? std::shared_lock<std::shared_mutex>(ongoingMutex) : std::shared_lock<std::shared_mutex>();
      ongoingMap::const_accessor b;
      auto txnItr = ongoing.find(b, msg.txn_digest());
      if(!txnItr){
      //if (txnItr == ongoing.end()) {
        Debug("PHASE2[%s] message does not contain txn, but have not seen"
            " txn_digest previously.", BytesToHex(msg.txn_digest(), 16).c_str());
        return;
      }

      //txn = txnItr->second;
      txn = b->second;
      b.release();
      txnDigest = &msg.txn_digest();
    } else {
      txn = &msg.txn();
      computedTxnDigest = TransactionDigest(msg.txn(), params.hashDigest);
      txnDigest = &computedTxnDigest;
    }
  }
  else{
    txnDigest = &dummyString;  //just so I can run with validate turned off while using params.mainThreadDispatching
  }

  proto::Phase2Reply* phase2Reply = GetUnusedPhase2Reply();
  TransportAddress *remoteCopy = remote.clone();

  auto sendCB = [this, remoteCopy, phase2Reply, txnDigest]() {
    //std::unique_lock<std::mutex> lock(transportMutex);
    this->transport->SendMessage(this, *remoteCopy, *phase2Reply);
    Debug("PHASE2[%s] Sent Phase2Reply.", BytesToHex(*txnDigest, 16).c_str());
    FreePhase2Reply(phase2Reply);
    delete remoteCopy;
  };
  auto cleanCB = [this, remoteCopy, phase2Reply]() {
    FreePhase2Reply(phase2Reply);
    delete remoteCopy;
  };

  // auto sendCB_wrapped = [this, remoteCopy, phase2Reply, txnDigest](void* arg) {
  //   //std::unique_lock<std::mutex> lock(transportMutex);
  //
  //   this->transport->SendMessage(this, *remoteCopy, *phase2Reply);
  //   Debug("PHASE2[%s] Sent Phase2Reply.", BytesToHex(*txnDigest, 16).c_str());
  //   FreePhase2Reply(phase2Reply);
  //   delete remoteCopy;
  // };
  //
  //
  // auto sendCB = [this, scb = std::move(sendCB_wrapped)]() {
  //   //std::unique_lock<std::mutex> lock(transportMutex);
  //   this->transport->IssueCB(scb, (void*) true);
  // };

  phase2Reply->set_req_id(msg.req_id());
  *phase2Reply->mutable_p2_decision()->mutable_txn_digest() = *txnDigest;
  phase2Reply->mutable_p2_decision()->set_involved_group(groupIdx);

  // no-replays property, i.e. recover existing decision/result from storage (do this for HandlePhase1 as well.)
   if(params.mainThreadDispatching) p2DecisionsMutex.lock();
  if(p2Decisions.find(msg.txn_digest()) != p2Decisions.end()){
     if(params.mainThreadDispatching) p2DecisionsMutex.unlock();
   //Logic moved to Callback:
            // proto::CommitDecision &decision =p2Decisions[msg.txn_digest()];
            // phase2Reply->mutable_p2_decision()->set_decision(decision);
            //  ADD VIEW. Is this the right notation?
            // if(decision_views.find(*txnDigest) == decision_views.end()) {
            //   decision_views[*txnDigest] = 0;
            // }
            //auto dec_view = decision_views[*txnDigest];
            //
            //phase2Reply->mutable_p2_decision()->set_view(dec_view);

  //first time message:
  } else{
     if(params.mainThreadDispatching) p2DecisionsMutex.unlock();
    Debug("PHASE2[%s].", BytesToHex(*txnDigest, 16).c_str());

    int64_t myProcessId;
    proto::ConcurrencyControl::Result myResult;
    LookupP1Decision(*txnDigest, myProcessId, myResult);


    if (params.validateProofs && params.signedMessages) {
        if(params.multiThreading){

          //Alternatively: parse into a new Phase2 directly in the ReceiveMessage function
          //proto::Phase2 * msg_copy = msg.New();
          //msg_copy->CopyFrom(msg);
          //copy shouldnt be needed due to bind

          mainThreadCallback mcb(std::bind(&Server::HandlePhase2CB, this,
             &msg, txnDigest, sendCB, phase2Reply, cleanCB, std::placeholders::_1));

          //OPTION 1: Validation itself is synchronous   Would need to be extended with thread safety.
              //std::function<void*()> f (std::bind(&ValidateP1RepliesWrapper, msg_copy.decision(), false, txn, txnDigest, msg.grouped_sigs(), keyManager, &config, myProcessId, myResult, verifier));
              //transport->DispatchTP(f, mcb);
              //tp->dispatch(f, cb, transport->libeventBase);

          //OPTION2: Validation itself is asynchronous (each verification = 1 job)
          if(params.batchVerification){
            asyncBatchValidateP1Replies(msg.decision(),
                  false, txn, txnDigest, msg.grouped_sigs(), keyManager, &config, myProcessId,
                  myResult, verifier, std::move(mcb), transport, true);
            return;

          }
          else{
            asyncValidateP1Replies(msg.decision(),
                  false, txn, txnDigest, msg.grouped_sigs(), keyManager, &config, myProcessId,
                  myResult, verifier, std::move(mcb), transport, true);
            return;
          }
        }
        else{
          if(params.batchVerification){
            //proto::Phase2 * msg_copy = msg.New();
            //msg_copy->CopyFrom(msg);

            //copy shouldnt be needed due to bind? or is it because msg is passed as reference?
            mainThreadCallback mcb(std::bind(&Server::HandlePhase2CB, this,
              &msg, txnDigest, sendCB, phase2Reply, cleanCB, std::placeholders::_1));

            asyncBatchValidateP1Replies(msg.decision(),
                  false, txn, txnDigest, msg.grouped_sigs(), keyManager, &config, myProcessId,
                  myResult, verifier, std::move(mcb), transport, false);
            return;
          }
          else{
            if(!ValidateP1Replies(msg.decision(),
                  false, txn, txnDigest, msg.grouped_sigs(), keyManager, &config, myProcessId,
                  myResult, verifier)) {
              Debug("VALIDATE P1Replies failed.");
              FreePhase2Reply(phase2Reply);
              delete remoteCopy;
              return;
            }
          }
        }
      }

  }
  // bool* valid = new bool(true);
  // HandlePhase2CB(msg, txnDigest, sendCB, phase2Reply, (void*) valid);

  // if(params.mainThreadDispatching) lockScope.unlock();
  HandlePhase2CB(&msg, txnDigest, sendCB, phase2Reply, cleanCB, (void*) true);

}

void Server::WritebackCallback(proto::Writeback *msg, const std::string* txnDigest,
  proto::Transaction* txn, void* valid){

  //auto lockScope = params.mainThreadDispatching ? std::unique_lock<std::mutex>(mainThreadMutex) : std::unique_lock<std::mutex>();

  auto f = [this, msg, txnDigest, txn, valid]() mutable {
  Debug("WRITEBACK Callback[%s] being called", BytesToHex(*txnDigest, 16).c_str());
  if(! valid){
    Debug("VALIDATE Writeback for TX %s failed.", BytesToHex(*txnDigest, 16).c_str());
    //delete (bool*) valid;
    if(params.multiThreading || params.mainThreadDispatching){
      FreeWBmessage(msg);
    }
    //return; //XXX comment back
    return (void*) false;
  }
  //delete (bool*) valid;

  ///////////////////////////// Below: Only executed by MainThread

  if (msg->decision() == proto::COMMIT) {
    Debug("WRITEBACK[%s] successfully committing.", BytesToHex(*txnDigest, 16).c_str());
    bool p1Sigs = msg->has_p1_sigs();
    uint64_t view = -1;
    if(!p1Sigs){
      if(msg->has_p2_sigs() && msg->has_p2_view()){
        view = msg->p2_view();
      }
      else{
        Debug("Writeback for P2 does not have view or sigs");
        //return; //XXX comment back
        view = 0;
        //return (void*) false;
      }
    }
    Debug("COMMIT ONLY RUN BY MAINTHREAD: %d", sched_getcpu());
    Commit(*txnDigest, txn, p1Sigs ? msg->release_p1_sigs() : msg->release_p2_sigs(), p1Sigs, view);
  } else {
    Debug("WRITEBACK[%s] successfully aborting.", BytesToHex(*txnDigest, 16).c_str());
    writebackMessages[*txnDigest] = *msg;
    ///TODO: msg might no longer hold txn; could have been released in HanldeWriteback
    Abort(*txnDigest);
  }

  if(params.multiThreading || params.mainThreadDispatching){
    // proto::GroupedSignatures* test = new proto::GroupedSignatures();
    // msg.set_allocated_p1_sigs(test);
    //delete msg;
    //proto::Writeback* msg2 = new proto::Writeback;
    FreeWBmessage(msg);
  }
   return (void*) true;
 };
 if(params.multiThreading && params.mainThreadDispatching && params.dispatchCallbacks){
   transport->DispatchTP_main(std::move(f));
 }
 else{
   f();
 }

}


void Server::HandleWriteback(const TransportAddress &remote,
    proto::Writeback &msg) {
  stats.Increment("total_transactions", 1);

  //auto lockScope = params.mainThreadDispatching ? std::unique_lock<std::mutex>(mainThreadMutex) : std::unique_lock<std::mutex>();
  proto::Transaction *txn;
  const std::string *txnDigest;
  std::string computedTxnDigest;
  if (!msg.has_txn_digest()) {
    Debug("WRITEBACK message contains neither txn nor txn_digest.");
    return;
  }

  UW_ASSERT(!msg.has_txn());

  if (msg.has_txn_digest()) {
    //auto ongoingMutexScope = params.mainThreadDispatching ? std::shared_lock<std::shared_mutex>(ongoingMutex) : std::shared_lock<std::shared_mutex>();
    ongoingMap::const_accessor b;
    auto txnItr = ongoing.find(b, msg.txn_digest());
    if(!txnItr){
    //if (txnItr == ongoing.end()) {
      Debug("WRITEBACK[%s] message does not contain txn, but have not seen"
          " txn_digest previously.", BytesToHex(msg.txn_digest(), 16).c_str());
      return;
    }

    //txn = txnItr->second;
    txn = b->second;
    b.release();
    txnDigest = &msg.txn_digest();
  } else {
    computedTxnDigest = TransactionDigest(msg.txn(), params.hashDigest);
    txn = msg.release_txn();
    txnDigest = &computedTxnDigest;  //Should make it async safe
  }

  Debug("WRITEBACK[%s] with decision %d.",
      BytesToHex(*txnDigest, 16).c_str(), msg.decision());

//could delegate this whole block to the threads, but then would need to make it threadsafe?

//TODO: declare mcb = bind(HandleWBCallback)

  if (params.validateProofs ) {
      if(params.multiThreading){
          //Alternatively: parse into a new Phase2 directly in the ReceiveMessage function
          //proto::Writeback * msg_copy = msg.New();
          //msg_copy->CopyFrom(msg);
          //copy shouldnt be needed due to bind?

          Debug("1: TAKING MULTITHREADING BRANCH, generating MCB");
          mainThreadCallback mcb(std::bind(&Server::WritebackCallback, this, &msg,
            txnDigest, txn, std::placeholders::_1));

          if(params.signedMessages && msg.decision() == proto::COMMIT && msg.has_p1_sigs()){
            stats.Increment("total_transactions_fast_commit", 1);
            int64_t myProcessId;
            proto::ConcurrencyControl::Result myResult;
            LookupP1Decision(*txnDigest, myProcessId, myResult);

            if(params.batchVerification){
              Debug("2: Taking batch branch p1 commit");
              asyncBatchValidateP1Replies(msg.decision(),
                    true, txn, txnDigest, msg.p1_sigs(), keyManager, &config, myProcessId,
                    myResult, verifier, std::move(mcb), transport, true);
            }
            else{
                  Debug("2: Taking non-batch branch p1 commit");
            asyncValidateP1Replies(msg.decision(),
                  true, txn, txnDigest, msg.p1_sigs(), keyManager, &config, myProcessId,
                  myResult, verifier, std::move(mcb), transport, true);
            }
            return;


          }
          else if(params.signedMessages && msg.decision() == proto::ABORT && msg.has_p1_sigs()){
            stats.Increment("total_transactions_fast_Abort_sigs", 1);
            int64_t myProcessId;
            proto::ConcurrencyControl::Result myResult;
            LookupP1Decision(*txnDigest, myProcessId, myResult);

            if(params.batchVerification){
              Debug("2: Taking batch branch p1 abort");
              asyncBatchValidateP1Replies(msg.decision(),
                    true, txn, txnDigest, msg.p1_sigs(), keyManager, &config, myProcessId,
                    myResult, verifier, std::move(mcb), transport, true);
            }
            else{
              Debug("2: Taking non-batch branch p1 abort");
            asyncValidateP1Replies(msg.decision(),
                  true, txn, txnDigest, msg.p1_sigs(), keyManager, &config, myProcessId,
                  myResult, verifier, std::move(mcb), transport, true);
            }
            return;
          }

          else if (params.signedMessages && msg.has_p2_sigs()) {
             stats.Increment("total_transactions_slow", 1);
              //TODO: Make async validate P2 replies func
              if(!msg.has_p2_view()) return;
              int64_t myProcessId;
              proto::CommitDecision myDecision;
              LookupP2Decision(*txnDigest, myProcessId, myDecision);

              if(params.batchVerification){
                Debug("2: Taking batch branch p2");
                asyncBatchValidateP2Replies(msg.decision(), msg.p2_view(),
                      txn, txnDigest, msg.p2_sigs(), keyManager, &config, myProcessId,
                      myDecision, verifier, std::move(mcb), transport, true);
              }
              else{
                Debug("2: Taking non-batch branch p2");
                asyncValidateP2Replies(msg.decision(), msg.p2_view(),
                      txn, txnDigest, msg.p2_sigs(), keyManager, &config, myProcessId,
                      myDecision, verifier, std::move(mcb), transport, true);
              }
              return;
          }


          else if (msg.decision() == proto::ABORT && msg.has_conflict()) {
             stats.Increment("total_transactions_fast_Abort_conflict", 1);
              //TODO:Make Async ValidateCommittedConflict
              std::string committedTxnDigest = TransactionDigest(msg.conflict().txn(),
                  params.hashDigest);
              asyncValidateCommittedConflict(msg.conflict(), &committedTxnDigest, txn,
                    txnDigest, params.signedMessages, keyManager, &config, verifier,
                    mcb, transport, true, params.batchVerification);
              return;
          }
          else if (params.signedMessages) {

             Debug("WRITEBACK[%s] decision %d, has_p1_sigs %d, has_p2_sigs %d, and"
                 " has_conflict %d.", BytesToHex(*txnDigest, 16).c_str(),
                 msg.decision(), msg.has_p1_sigs(), msg.has_p2_sigs(), msg.has_conflict());
             return;
          }

      }
      //If I make the else case use the async function too, then I can collapse the duplicate code here
      //and just pass params.multiThreading as argument...
      //Currently NOT doing that because the async version does additional copies (binds) that could be avoided?
      else{

          if (params.signedMessages && msg.decision() == proto::COMMIT && msg.has_p1_sigs()) {
            int64_t myProcessId;
            proto::ConcurrencyControl::Result myResult;
            LookupP1Decision(*txnDigest, myProcessId, myResult);

            if(params.batchVerification){
              //proto::Writeback * msg_copy = msg.New();
              //msg_copy->CopyFrom(msg);
              //copy shouldnt be needed due to bind?
              mainThreadCallback mcb(std::bind(&Server::WritebackCallback, this, &msg, txnDigest, txn, std::placeholders::_1));
              asyncBatchValidateP1Replies(proto::COMMIT,
                    true, txn, txnDigest,msg.p1_sigs(), keyManager, &config, myProcessId,
                    myResult, verifier, std::move(mcb), transport, false);
              return;
            }
            else{
              if (!ValidateP1Replies(proto::COMMIT, true, txn, txnDigest, msg.p1_sigs(),
                    keyManager, &config, myProcessId, myResult, verifyLat, verifier)) {
                Debug("WRITEBACK[%s] Failed to validate P1 replies for fast commit.",
                    BytesToHex(*txnDigest, 16).c_str());
                return;
              }
            }
          }
          //inerted previously MISSING CASE FOR FAST ABORT WITH 3f+1 ABSTAIN
          //TODO: need to check whether clients even ever generate this (they should) (+ server mvtso replies)
          else if (params.signedMessages && msg.decision() == proto::ABORT && msg.has_p1_sigs()) {
            int64_t myProcessId;
            proto::ConcurrencyControl::Result myResult;
            LookupP1Decision(*txnDigest, myProcessId, myResult);

            if(params.batchVerification){
              //proto::Writeback * msg_copy = msg.New();
              //msg_copy->CopyFrom(msg);
              //copy shouldnt be needed due to bind?
              mainThreadCallback mcb(std::bind(&Server::WritebackCallback, this, &msg, txnDigest, txn, std::placeholders::_1));
              asyncBatchValidateP1Replies(proto::ABORT,
                    true, txn, txnDigest,msg.p1_sigs(), keyManager, &config, myProcessId,
                    myResult, verifier, std::move(mcb), transport, false);
              return;
            }
            else{
              if (!ValidateP1Replies(proto::ABORT, true, txn, txnDigest, msg.p1_sigs(),
                    keyManager, &config, myProcessId, myResult, verifyLat, verifier)) {
                Debug("WRITEBACK[%s] Failed to validate P1 replies for fast abort.",
                    BytesToHex(*txnDigest, 16).c_str());
                return;
              }
            }
          }


          else if (params.signedMessages && msg.has_p2_sigs()) {
            if(!msg.has_p2_view()) return;
            int64_t myProcessId;
            proto::CommitDecision myDecision;
            LookupP2Decision(*txnDigest, myProcessId, myDecision);


            if(params.batchVerification){
              //proto::Writeback * msg_copy = msg.New();
              //msg_copy->CopyFrom(msg);
              //copy shouldnt be needed due to bind?
              mainThreadCallback mcb(std::bind(&Server::WritebackCallback, this, &msg, txnDigest, txn, std::placeholders::_1));
              asyncBatchValidateP2Replies(msg.decision(), msg.p2_view(),
                    txn, txnDigest, msg.p2_sigs(), keyManager, &config, myProcessId,
                    myDecision, verifier, std::move(mcb), transport, false);
              return;
            }
            else{
              if (!ValidateP2Replies(msg.decision(), msg.p2_view(), txn, txnDigest, msg.p2_sigs(),
                    keyManager, &config, myProcessId, myDecision, verifyLat, verifier)) {
                Debug("WRITEBACK[%s] Failed to validate P2 replies for decision %d.",
                    BytesToHex(*txnDigest, 16).c_str(), msg.decision());
                return;
              }
            }

          } else if (msg.decision() == proto::ABORT && msg.has_conflict()) {
            std::string committedTxnDigest = TransactionDigest(msg.conflict().txn(),
                params.hashDigest);

                if(params.batchVerification){
                  //proto::Writeback * msg_copy = msg.New();
                  //msg_copy->CopyFrom(msg);
                  //copy shouldnt be needed due to bind?
                  mainThreadCallback mcb(std::bind(&Server::WritebackCallback, this, &msg, txnDigest, txn, std::placeholders::_1));
                  asyncValidateCommittedConflict(msg.conflict(), &committedTxnDigest, txn,
                        txnDigest, params.signedMessages, keyManager, &config, verifier,
                        std::move(mcb), transport, false, params.batchVerification);
                  return;
                }
                else{
                  if (!ValidateCommittedConflict(msg.conflict(), &committedTxnDigest, txn,
                        txnDigest, params.signedMessages, keyManager, &config, verifier)) {
                    Debug("WRITEBACK[%s] Failed to validate committed conflict for fast abort.",
                        BytesToHex(*txnDigest, 16).c_str());
                    return;
                }

            }
          } else if (params.signedMessages) {
            Debug("WRITEBACK[%s] decision %d, has_p1_sigs %d, has_p2_sigs %d, and"
                " has_conflict %d.", BytesToHex(*txnDigest, 16).c_str(),
                msg.decision(), msg.has_p1_sigs(), msg.has_p2_sigs(), msg.has_conflict());
            return;
          }
        }

  }
  // bool* valid = new bool(true);
  // WritebackCallback(msg, txnDigest, txn, (void*) valid);
  // if(params.mainThreadDispatching) lockScope.unlock();
  WritebackCallback(&msg, txnDigest, txn, (void*) true);
}


void Server::HandleAbort(const TransportAddress &remote,
    const proto::Abort &msg) {
  const proto::AbortInternal *abort;
  if (params.validateProofs && params.signedMessages) {
    if (!msg.has_signed_internal()) {
      return;
    }

    //Latency_Start(&verifyLat);
    if (!verifier->Verify(keyManager->GetPublicKey(msg.signed_internal().process_id()),
          msg.signed_internal().data(),
          msg.signed_internal().signature())) {
      //Latency_End(&verifyLat);
      return;
    }
    //Latency_End(&verifyLat);

    if (!abortInternal.ParseFromString(msg.signed_internal().data())) {
      return;
    }

    if (abortInternal.ts().id() != msg.signed_internal().process_id()) {
      return;
    }

    abort = &abortInternal;
  } else {
    UW_ASSERT(msg.has_internal());
    abort = &msg.internal();
  }

  //RECOMMENT
  //  if(params.mainThreadDispatching) rtsMutex.lock();
  // for (const auto &read : abort->read_set()) {
  //   rts[read].erase(abort->ts());
  // }
  //  if(params.mainThreadDispatching) rtsMutex.unlock();
}

proto::ConcurrencyControl::Result Server::DoOCCCheck(
    uint64_t reqId, const TransportAddress &remote,
    const std::string &txnDigest, const proto::Transaction &txn,
    Timestamp &retryTs, const proto::CommittedProof* &conflict) {
  switch (occType) {
    case TAPIR:
      return DoTAPIROCCCheck(txnDigest, txn, retryTs);
    case MVTSO:
      return DoMVTSOOCCCheck(reqId, remote, txnDigest, txn, conflict);
    default:
      Panic("Unknown OCC type: %d.", occType);
      return proto::ConcurrencyControl::ABORT;
  }
}

void Server::LockTxnKeys(proto::Transaction &txn){
  // Lock all (read/write) keys in order for atomicity if using parallel OCC
    auto itr_r = txn.read_set().begin();
    auto itr_w = txn.write_set().begin();
    for(int i = 0; i < txn.read_set().size() + txn.write_set().size(); ++i){
      if(itr_r == txn.read_set().end()){
        lock_keys[itr_w->key()].lock();
        itr_w++;
      }
      else if(itr_w == txn.write_set().end()){
        lock_keys[itr_r->key()].lock();
        itr_r++;
      }
      else{
        if(itr_r->key() < itr_w->key()){
          lock_keys[itr_r->key()].lock();
          itr_r++;
        }
        else{
          lock_keys[itr_w->key()].lock();
          itr_w++;
        }
      }
    }
}
void Server::UnlockTxnKeys(proto::Transaction &txn){
  // Lock all (read/write) keys in order for atomicity if using parallel OCC
    auto itr_r = txn.read_set().rbegin();
    auto itr_w = txn.write_set().rbegin();
    for(int i = 0; i < txn.read_set().size() + txn.write_set().size(); ++i){
      if(itr_r == txn.read_set().rend()){
        lock_keys[itr_w->key()].unlock();
        itr_w++;
      }
      else if(itr_w == txn.write_set().rend()){
        lock_keys[itr_r->key()].unlock();
        itr_r++;
      }
      else{
        if(itr_r->key() >= itr_w->key()){
          lock_keys[itr_r->key()].unlock();
          itr_r++;
        }
        else{
          lock_keys[itr_w->key()].unlock();
          itr_w++;
        }
      }
    }
}

proto::ConcurrencyControl::Result Server::DoTAPIROCCCheck(
    const std::string &txnDigest, const proto::Transaction &txn,
    Timestamp &retryTs) {
  Debug("[%s] START PREPARE", txnDigest.c_str());


  if(params.mainThreadDispatching) preparedMutex.lock_shared();
  preparedMap::const_accessor a;
  auto preparedItr = prepared.find(a, txnDigest);
  if(preparedItr){
      if(a->second.first == txn.timestamp()){
  //if (preparedItr != prepared.end()) {
  //  if (preparedItr->second.first == txn.timestamp()) {
      Warning("[%s] Already Prepared!", txnDigest.c_str());
      if(params.mainThreadDispatching) preparedMutex.unlock_shared();
      return proto::ConcurrencyControl::COMMIT;
    } else {
      // run the checks again for a new timestamp
      if(params.mainThreadDispatching) preparedMutex.unlock_shared();
      Clean(txnDigest);
    }
  }
  a.release();
  // do OCC checks
  std::unordered_map<std::string, std::set<Timestamp>> pReads;
  GetPreparedReadTimestamps(pReads);

  // check for conflicts with the read set
  for (const auto &read : txn.read_set()) {
    std::pair<Timestamp, Timestamp> range;
     //if(params.mainThreadDispatching) storeMutex.lock();
    bool ret = store.getRange(read.key(), read.readtime(), range);
     //if(params.mainThreadDispatching) storeMutex.unlock();

    Debug("Range %lu %lu %lu", Timestamp(read.readtime()).getTimestamp(),
        range.first.getTimestamp(), range.second.getTimestamp());

    // if we don't have this key then no conflicts for read
    if (!ret) {
      continue;
    }

    // if we don't have this version then no conflicts for read
    if (range.first != read.readtime()) {
      continue;
    }

    // if the value is still valid
    if (!range.second.isValid()) {
      // check pending writes.
      //auto preparedWritesMutexScope = params.mainThreadDispatching ? std::shared_lock<std::shared_mutex>(preparedWritesMutex) : std::shared_lock<std::shared_mutex>();

      if (preparedWrites.find(read.key()) != preparedWrites.end()) {
        Debug("[%lu,%lu] ABSTAIN rw conflict w/ prepared key %s.",
            txn.client_id(),
            txn.client_seq_num(),
            BytesToHex(read.key(), 16).c_str());
        stats.Increment("cc_abstains", 1);
        stats.Increment("cc_abstains_rw_conflict", 1);
        return proto::ConcurrencyControl::ABSTAIN;
      }
    } else {
      // if value is not still valtxnDigest, then abort.
      /*if (Timestamp(txn.timestamp()) <= range.first) {
        Warning("timestamp %lu <= range.first %lu (range.second %lu)",
            txn.timestamp().timestamp(), range.first.getTimestamp(),
            range.second.getTimestamp());
      }*/
      //UW_ASSERT(timestamp > range.first);
      Debug("[%s] ABORT rw conflict: %lu > %lu", txnDigest.c_str(),
          txn.timestamp().timestamp(), range.second.getTimestamp());
      stats.Increment("cc_aborts", 1);
      stats.Increment("cc_aborts_rw_conflict", 1);
      return proto::ConcurrencyControl::ABORT;
    }
  }

  // check for conflicts with the write set
  for (const auto &write : txn.write_set()) {
    std::pair<Timestamp, Server::Value> val;
    // if this key is in the store
     //if(params.mainThreadDispatching) storeMutex.lock();
    if (store.get(write.key(), val)) {
      Timestamp lastRead;
      bool ret;

      // if the last committed write is bigger than the timestamp,
      // then can't accept
      if (val.first > Timestamp(txn.timestamp())) {
        Debug("[%s] RETRY ww conflict w/ prepared key:%s", txnDigest.c_str(),
            write.key().c_str());
        retryTs = val.first;
        stats.Increment("cc_retries_committed_write", 1);
         //if(params.mainThreadDispatching) storeMutex.unlock();
        return proto::ConcurrencyControl::ABSTAIN;
      }

      // if last committed read is bigger than the timestamp, can't
      // accept this transaction, but can propose a retry timestamp

      // we get the timestamp of the last read ever on this object
      ret = store.getLastRead(write.key(), lastRead);

      // if this key is in the store and has been read before
      if (ret && lastRead > Timestamp(txn.timestamp())) {
        Debug("[%s] RETRY wr conflict w/ prepared key:%s", txnDigest.c_str(),
            write.key().c_str());
        retryTs = lastRead;
         //if(params.mainThreadDispatching) storeMutex.unlock();
        return proto::ConcurrencyControl::ABSTAIN;
      }
    }
     //if(params.mainThreadDispatching) storeMutex.unlock();

    // if there is a pending write for this key, greater than the
    // proposed timestamp, retry

     //if(params.mainThreadDispatching) preparedWritesMutex.lock_shared();

     auto itr = preparedWrites.find(write.key());

    if (itr != preparedWrites.end()) {
      itr->second.first.lock_shared();
      std::map<Timestamp, const proto::Transaction *>::iterator it =
          itr->second.second.upper_bound(txn.timestamp());
      if (it != itr->second.second.end() ) {
        Debug("[%s] RETRY ww conflict w/ prepared key:%s", txnDigest.c_str(),
            write.key().c_str());
        retryTs = it->first;
        stats.Increment("cc_retries_prepared_write", 1);
         //if(params.mainThreadDispatching) preparedWritesMutex.unlock_shared();
         itr->second.first.unlock_shared();
        return proto::ConcurrencyControl::ABSTAIN;
      }
      itr->second.first.unlock_shared();
    }
     //if(params.mainThreadDispatching) preparedWritesMutex.unlock_shared();


    //if there is a pending read for this key, greater than the
    //propsed timestamp, abstain
    if (pReads.find(write.key()) != pReads.end() &&
        pReads[write.key()].upper_bound(txn.timestamp()) !=
        pReads[write.key()].end()) {
      Debug("[%s] ABSTAIN wr conflict w/ prepared key: %s",
            txnDigest.c_str(), write.key().c_str());
      stats.Increment("cc_abstains", 1);
      return proto::ConcurrencyControl::ABSTAIN;
    }
  }

  // Otherwise, prepare this transaction for commit
  Prepare(txnDigest, txn);

  Debug("[%s] PREPARED TO COMMIT", txnDigest.c_str());

  return proto::ConcurrencyControl::COMMIT;
}

proto::ConcurrencyControl::Result Server::DoMVTSOOCCCheck(
    uint64_t reqId, const TransportAddress &remote,
    const std::string &txnDigest, const proto::Transaction &txn,
    const proto::CommittedProof* &conflict) {
  Debug("PREPARE[%lu:%lu][%s] with ts %lu.%lu.",
      txn.client_id(), txn.client_seq_num(),
      BytesToHex(txnDigest, 16).c_str(),
      txn.timestamp().timestamp(), txn.timestamp().id());
  Timestamp ts(txn.timestamp());

  //Latency_Start(&waitingOnLocks);
   //if(params.mainThreadDispatching) preparedMutex.lock_shared();
  //Latency_End(&waitingOnLocks);

  preparedMap::const_accessor a;
  bool preparedItr = prepared.find(a, txnDigest);
  //if (preparedItr == prepared.end()) {
  //TODO need to lock before, so 2 parallel OCC dont accidentally set a decision twice.
  if(!preparedItr){
    a.release();
    if (CheckHighWatermark(ts)) {
      Debug("[%lu:%lu][%s] ABSTAIN ts %lu beyond high watermark.",
          txn.client_id(), txn.client_seq_num(),
          BytesToHex(txnDigest, 16).c_str(),
          ts.getTimestamp());
      stats.Increment("cc_abstains", 1);
      stats.Increment("cc_abstains_watermark", 1);
       //if(params.mainThreadDispatching) preparedMutex.unlock_shared();
      return proto::ConcurrencyControl::ABSTAIN;
    }
     //if(params.mainThreadDispatching) preparedMutex.unlock_shared();

    for (const auto &read : txn.read_set()) {
      // TODO: remove this check when txns only contain read set/write set for the
      //   shards stored at this replica
      if (!IsKeyOwned(read.key())) {
        continue;
      }

      std::vector<std::pair<Timestamp, Server::Value>> committedWrites;
      GetCommittedWrites(read.key(), read.readtime(), committedWrites);
      for (const auto &committedWrite : committedWrites) {
        // readVersion < committedTs < ts
        //     GetCommittedWrites only returns writes larger than readVersion
        if (committedWrite.first < ts) {
          if (params.validateProofs) {
            conflict = committedWrite.second.proof;
          }
          Debug("[%lu:%lu][%s] ABORT wr conflict committed write for key %s:"
              " this txn's read ts %lu.%lu < committed ts %lu.%lu < this txn's ts %lu.%lu.",
              txn.client_id(),
              txn.client_seq_num(),
              BytesToHex(txnDigest, 16).c_str(),
              BytesToHex(read.key(), 16).c_str(),
              read.readtime().timestamp(),
              read.readtime().id(), committedWrite.first.getTimestamp(),
              committedWrite.first.getID(), ts.getTimestamp(), ts.getID());
          stats.Increment("cc_aborts", 1);
          stats.Increment("cc_aborts_wr_conflict", 1);
          return proto::ConcurrencyControl::ABORT;
        }
      }
      //Latency_Start(&waitingOnLocks);
       //if(params.mainThreadDispatching) preparedWritesMutex.lock_shared();
      //Latency_End(&waitingOnLocks);

      const auto preparedWritesItr = preparedWrites.find(read.key());
      if (preparedWritesItr != preparedWrites.end()) {

        std::shared_lock lock(preparedWritesItr->second.first);
        for (const auto &preparedTs : preparedWritesItr->second.second) {
          if (Timestamp(read.readtime()) < preparedTs.first && preparedTs.first < ts) {
            Debug("[%lu:%lu][%s] ABSTAIN wr conflict prepared write for key %s:"
              " this txn's read ts %lu.%lu < prepared ts %lu.%lu < this txn's ts %lu.%lu.",
                txn.client_id(),
                txn.client_seq_num(),
                BytesToHex(txnDigest, 16).c_str(),
                BytesToHex(read.key(), 16).c_str(),
                read.readtime().timestamp(),
                read.readtime().id(), preparedTs.first.getTimestamp(),
                preparedTs.first.getID(), ts.getTimestamp(), ts.getID());
            stats.Increment("cc_abstains", 1);
            stats.Increment("cc_abstains_wr_conflict", 1);
             //if(params.mainThreadDispatching) preparedWritesMutex.unlock_shared();
            return proto::ConcurrencyControl::ABSTAIN;
          }
        }
      }
       //if(params.mainThreadDispatching) preparedWritesMutex.unlock_shared();
    }

    for (const auto &write : txn.write_set()) {
      if (!IsKeyOwned(write.key())) {
        continue;
      }
      //Latency_Start(&waitingOnLocks);
       //if(params.mainThreadDispatching) committedReadsMutex.lock_shared();
      //Latency_End(&waitingOnLocks);
      auto committedReadsItr = committedReads.find(write.key());

      if (committedReadsItr != committedReads.end()){
         std::shared_lock lock(committedReadsItr->second.first);
         if(committedReadsItr->second.second.size() > 0) {
          for (auto ritr = committedReadsItr->second.second.rbegin();
              ritr != committedReadsItr->second.second.rend(); ++ritr) {
            if (ts >= std::get<0>(*ritr)) {
              // iterating over committed reads from largest to smallest committed txn ts
              //    if ts is larger than current itr, it is also larger than all subsequent itrs
              break;
            } else if (std::get<1>(*ritr) < ts) {
              if (params.validateProofs) {
                conflict = std::get<2>(*ritr);
              }
              Debug("[%lu:%lu][%s] ABORT rw conflict committed read for key %s: committed"
                  " read ts %lu.%lu < this txn's ts %lu.%lu < committed ts %lu.%lu.",
                  txn.client_id(),
                  txn.client_seq_num(),
                  BytesToHex(txnDigest, 16).c_str(),
                  BytesToHex(write.key(), 16).c_str(),
                  std::get<1>(*ritr).getTimestamp(),
                  std::get<1>(*ritr).getID(), ts.getTimestamp(),
                  ts.getID(), std::get<0>(*ritr).getTimestamp(),
                  std::get<0>(*ritr).getID());
              stats.Increment("cc_aborts", 1);
              stats.Increment("cc_aborts_rw_conflict", 1);
               //if(params.mainThreadDispatching) committedReadsMutex.unlock_shared();
              return proto::ConcurrencyControl::ABORT;
            }
          }
        }
      }
       //if(params.mainThreadDispatching) committedReadsMutex.unlock_shared();

       //Latency_Start(&waitingOnLocks);
       //if(params.mainThreadDispatching) preparedReadsMutex.lock_shared();
       //Latency_End(&waitingOnLocks);
      const auto preparedReadsItr = preparedReads.find(write.key());
      if (preparedReadsItr != preparedReads.end()) {

        std::shared_lock lock(preparedReadsItr->second.first);

        for (const auto preparedReadTxn : preparedReadsItr->second.second) {
          bool isDep = false;
          for (const auto &dep : preparedReadTxn->deps()) {
            if (txnDigest == dep.write().prepared_txn_digest()) {
              isDep = true;
              break;
            }
          }

          bool isReadVersionEarlier = false;
          Timestamp readTs;
          for (const auto &read : preparedReadTxn->read_set()) {
            if (read.key() == write.key()) {
              readTs = Timestamp(read.readtime());
              isReadVersionEarlier = readTs < ts;
              break;
            }
          }
          if (!isDep && isReadVersionEarlier &&
              ts < Timestamp(preparedReadTxn->timestamp())) {
            Debug("[%lu:%lu][%s] ABSTAIN rw conflict prepared read for key %s: prepared"
                " read ts %lu.%lu < this txn's ts %lu.%lu < committed ts %lu.%lu.",
                txn.client_id(),
                txn.client_seq_num(),
                BytesToHex(txnDigest, 16).c_str(),
                BytesToHex(write.key(), 16).c_str(),
                readTs.getTimestamp(),
                readTs.getID(), ts.getTimestamp(),
                ts.getID(), preparedReadTxn->timestamp().timestamp(),
                preparedReadTxn->timestamp().id());
            stats.Increment("cc_abstains", 1);
            stats.Increment("cc_abstains_rw_conflict", 1);
             //if(params.mainThreadDispatching) preparedReadsMutex.unlock_shared();
            return proto::ConcurrencyControl::ABSTAIN;
          }
        }
      }
       //if(params.mainThreadDispatching) preparedReadsMutex.unlock_shared();

       //RECOMMENT
      //  //Latency_Start(&waitingOnLocks);
      //  if(params.mainThreadDispatching) rtsMutex.lock_shared();
      //  //Latency_End(&waitingOnLocks);
      // auto rtsItr = rts.find(write.key());
      // if (rtsItr != rts.end()) {
      //   auto rtsRBegin = rtsItr->second.rbegin();
      //   if (rtsRBegin != rtsItr->second.rend()) {
      //     Debug("Largest rts for write to key %s: %lu.%lu.",
      //       BytesToHex(write.key(), 16).c_str(), rtsRBegin->getTimestamp(),
      //       rtsRBegin->getID());
      //   }
      //   auto rtsLB = rtsItr->second.lower_bound(ts);
      //   if (rtsLB != rtsItr->second.end()) {
      //     Debug("Lower bound rts for write to key %s: %lu.%lu.",
      //       BytesToHex(write.key(), 16).c_str(), rtsLB->getTimestamp(),
      //       rtsLB->getID());
      //     if (*rtsLB == ts) {
      //       rtsLB++;
      //     }
      //     if (rtsLB != rtsItr->second.end()) {
      //       if (*rtsLB > ts) {
      //         Debug("[%lu:%lu][%s] ABSTAIN larger rts acquired for key %s: rts %lu.%lu >"
      //             " this txn's ts %lu.%lu.",
      //             txn.client_id(),
      //             txn.client_seq_num(),
      //             BytesToHex(txnDigest, 16).c_str(),
      //             BytesToHex(write.key(), 16).c_str(),
      //             rtsLB->getTimestamp(),
      //             rtsLB->getID(), ts.getTimestamp(), ts.getID());
      //         stats.Increment("cc_abstains", 1);
      //         stats.Increment("cc_abstains_rts", 1);
      //          if(params.mainThreadDispatching) rtsMutex.unlock_shared();
      //         return proto::ConcurrencyControl::ABSTAIN;
      //       }
      //     }
      //   }
      // }
      //  if(params.mainThreadDispatching) rtsMutex.unlock_shared();
      auto rtsItr = rts.find(write.key());
      if(rtsItr != rts.end()){
        if(rtsItr->second > ts.getTimestamp()){
          ///TODO XXX Re-introduce ID also, for finer ordering. This is safe, since the
          //RTS check is just an additional heuristic; The prepare/commit checks guarantee serializability on their own
          stats.Increment("cc_abstains", 1);
          stats.Increment("cc_abstains_rts", 1);
          return proto::ConcurrencyControl::ABSTAIN;
        }
      }

      // TODO: add additional rts dep check to shrink abort window
      //    Is this still a thing?
    }

    if (params.validateProofs && params.signedMessages && !params.verifyDeps) {
      //Latency_Start(&waitingOnLocks);
      //auto committedMutexScope = params.mainThreadDispatching ? std::shared_lock<std::shared_mutex>(committedMutex) : std::shared_lock<std::shared_mutex>();
      //auto abortedMutexScope = params.mainThreadDispatching ? std::shared_lock<std::shared_mutex>(abortedMutex) : std::shared_lock<std::shared_mutex>();
      //auto preparedMutexScope = params.mainThreadDispatching ? std::shared_lock<std::shared_mutex>(preparedMutex) : std::shared_lock<std::shared_mutex>();
      //Latency_End(&waitingOnLocks);

      Debug("Exec MessageToSign by CPU: %d", sched_getcpu());
      for (const auto &dep : txn.deps()) {
        if (dep.involved_group() != groupIdx) {
          continue;
        }
        if (committed.find(dep.write().prepared_txn_digest()) == committed.end() &&
            aborted.find(dep.write().prepared_txn_digest()) == aborted.end()) {
          //check whether we have prepared it ourselves: This alleviates having to verify dep proofs

          preparedMap::const_accessor a2;
          auto preparedItr = prepared.find(a2, dep.write().prepared_txn_digest());
          if(!preparedItr){
          //if (preparedItr == prepared.end()) {
            return proto::ConcurrencyControl::ABSTAIN;
          }
          a2.release();
        }
      }
    }
    Prepare(txnDigest, txn);
  }
  else{
     a.release();
     //if(params.mainThreadDispatching) preparedMutex.unlock_shared();
  }

  bool allFinished = true;

  if(params.maxDepDepth > -2){
    //Latency_Start(&waitingOnLocks);
     if(params.mainThreadDispatching) dependentsMutex.lock();
     if(params.mainThreadDispatching) waitingDependenciesMutex.lock();
     //if(params.mainThreadDispatching) ongoingMutex.lock_shared();
     //if(params.mainThreadDispatching) committedMutex.lock_shared();
     //if(params.mainThreadDispatching) abortedMutex.lock_shared();
    //Latency_End(&waitingOnLocks);


    for (const auto &dep : txn.deps()) {
      if (dep.involved_group() != groupIdx) {
        continue;
      }
      if (committed.find(dep.write().prepared_txn_digest()) == committed.end() &&
          aborted.find(dep.write().prepared_txn_digest()) == aborted.end()) {
        Debug("[%lu:%lu][%s] WAIT for dependency %s to finish.",
            txn.client_id(), txn.client_seq_num(),
            BytesToHex(txnDigest, 16).c_str(),
            BytesToHex(dep.write().prepared_txn_digest(), 16).c_str());

        //start RelayP1 to initiate Fallback handling
        //TODO: Currently disabled.
        // if (false && ongoing.find(dep.write().prepared_txn_digest()) != ongoing.end()) {
        //   std::string txnDig = dep.write().prepared_txn_digest();
        //   //schedule Relay for client timeout only..
        //   //proto::Transaction *tx = ongoing[txnDig];
        //   transport->Timer((CLIENTTIMEOUT), [this, &remote, txnDig, reqId](){this->RelayP1(remote, txnDig, reqId);});
        //   //RelayP1(remote, *tx, reqId);
        // }
        allFinished = false;
        std::cout << "ABORTING AT 1" << std::endl;
        dependentsMap::accessor e;
        //dependents[dep.write().prepared_txn_digest()].insert(e, txnDigest);
        dependents.insert(e, dep.write().prepared_txn_digest());
        e->second.insert(txnDigest);
        e.release();
        auto dependenciesItr = waitingDependencies.find(txnDigest);
        if (dependenciesItr == waitingDependencies.end()) {
          auto inserted = waitingDependencies.insert(std::make_pair(txnDigest,
                WaitingDependency()));
          UW_ASSERT(inserted.second);
          dependenciesItr = inserted.first;
        }
        dependenciesItr->second.reqId = reqId;
        dependenciesItr->second.remote = remote.clone();  //&remote;
        dependenciesItr->second.deps.insert(dep.write().prepared_txn_digest());
      }
    }

     if(params.mainThreadDispatching) dependentsMutex.unlock();
     if(params.mainThreadDispatching) waitingDependenciesMutex.unlock();
     //if(params.mainThreadDispatching) ongoingMutex.unlock_shared();
     //if(params.mainThreadDispatching) committedMutex.unlock_shared();
     //if(params.mainThreadDispatching) abortedMutex.unlock_shared();
 }

  if (!allFinished) {
    stats.Increment("cc_waits", 1);
    return proto::ConcurrencyControl::WAIT;
  } else {
    return CheckDependencies(txn);
  }
}

void Server::GetPreparedReadTimestamps(
    std::unordered_map<std::string, std::set<Timestamp>> &reads) {
  // gather up the set of all writes that are currently prepared
   if(params.mainThreadDispatching) preparedMutex.lock_shared();
  for (const auto &t : prepared) {
    for (const auto &read : t.second.second->read_set()) {
      if (IsKeyOwned(read.key())) {
        reads[read.key()].insert(t.second.first);
      }
    }
  }
   if(params.mainThreadDispatching) preparedMutex.unlock_shared();
}

void Server::GetPreparedReads(
    std::unordered_map<std::string, std::vector<const proto::Transaction*>> &reads) {
  // gather up the set of all writes that are currently prepared
   if(params.mainThreadDispatching) preparedMutex.lock_shared();
  for (const auto &t : prepared) {
    for (const auto &read : t.second.second->read_set()) {
      if (IsKeyOwned(read.key())) {
        reads[read.key()].push_back(t.second.second);
      }
    }
  }
   if(params.mainThreadDispatching) preparedMutex.unlock_shared();
}

void Server::Prepare(const std::string &txnDigest,
    const proto::Transaction &txn) {
  Debug("PREPARE[%s] agreed to commit with ts %lu.%lu.",
      BytesToHex(txnDigest, 16).c_str(), txn.timestamp().timestamp(), txn.timestamp().id());


  //auto ongoingMutexScope = params.mainThreadDispatching ? std::shared_lock<std::shared_mutex>(ongoingMutex) : std::shared_lock<std::shared_mutex>();
  //auto preparedMutexScope = params.mainThreadDispatching ? std::unique_lock<std::shared_mutex>(preparedMutex) : std::unique_lock<std::shared_mutex>();
  //auto preparedReadsMutexScope = params.mainThreadDispatching ? std::unique_lock<std::shared_mutex>(preparedReadsMutex) : std::unique_lock<std::shared_mutex>();
  //auto preparedWritesMutexScope = params.mainThreadDispatching ? std::unique_lock<std::shared_mutex>(preparedWritesMutex) : std::unique_lock<std::shared_mutex>();

  ongoingMap::const_accessor b;
  auto ongoingItr = ongoing.find(b, txnDigest);
  if(!ongoingItr){
  //if(ongoingItr == ongoing.end()){
    Debug("Already concurrently Committed/Aborted txn[%s]", BytesToHex(txnDigest, 16).c_str());
    return;
  }
  const proto::Transaction *ongoingTxn = b->second;
  //const proto::Transaction *ongoingTxn = ongoing.at(txnDigest);


  preparedMap::accessor a;
  auto p = prepared.insert(a, std::make_pair(txnDigest, std::make_pair(
          Timestamp(txn.timestamp()), ongoingTxn)));
  b.release();

  for (const auto &read : txn.read_set()) {
    if (IsKeyOwned(read.key())) {
      //preparedReads[read.key()].insert(p.first->second.second);
      //preparedReads[read.key()].insert(a->second.second);

      std::pair<std::shared_mutex, std::set<const proto::Transaction *>> &y = preparedReads[read.key()];
      std::unique_lock lock(y.first);
      y.second.insert(a->second.second);

    }
  }

  std::pair<Timestamp, const proto::Transaction *> pWrite =
    std::make_pair(a->second.first, a->second.second);
  a.release();
    //std::make_pair(p.first->second.first, p.first->second.second);
  for (const auto &write : txn.write_set()) {
    if (IsKeyOwned(write.key())) {
      std::pair<std::shared_mutex,std::map<Timestamp, const proto::Transaction *>> &x = preparedWrites[write.key()];
      std::unique_lock lock(x.first);
      x.second.insert(pWrite);
      // std::unique_lock lock(preparedWrites[write.key()].first);
      // preparedWrites[write.key()].second.insert(pWrite);
    }
  }

}

void Server::GetCommittedWrites(const std::string &key, const Timestamp &ts,
    std::vector<std::pair<Timestamp, Server::Value>> &writes) {
  std::vector<std::pair<Timestamp, Server::Value>> values;
   //if(params.mainThreadDispatching) storeMutex.lock();
  if (store.getCommittedAfter(key, ts, values)) {
    for (const auto &p : values) {
      writes.push_back(p);
    }
  }
   //if(params.mainThreadDispatching) storeMutex.unlock();
}

void Server::Commit(const std::string &txnDigest, proto::Transaction *txn,
      proto::GroupedSignatures *groupedSigs, bool p1Sigs, uint64_t view) {

  Timestamp ts(txn->timestamp());

  Value val;
  proto::CommittedProof *proof = nullptr;
  if (params.validateProofs) {
    Debug("Access only by CPU: %d", sched_getcpu());
    proof = new proto::CommittedProof();
    //proof = testing_committed_proof.back();
    //testing_committed_proof.pop_back();
  }
  val.proof = proof;
  //Latency_Start(&waitingOnLocks);
   //if(params.mainThreadDispatching) committedMutex.lock();
  //Latency_End(&waitingOnLocks);
  auto committedItr = committed.insert(std::make_pair(txnDigest, proof));
  //auto committedItr =committed.emplace(txnDigest, proof);
   //if(params.mainThreadDispatching) committedMutex.unlock();

  if (params.validateProofs) {
    // CAUTION: we no longer own txn pointer (which we allocated during Phase1
    //    and stored in ongoing)
    proof->set_allocated_txn(txn);
    if (params.signedMessages) {
      if (p1Sigs) {
        proof->set_allocated_p1_sigs(groupedSigs);
      } else {
        proof->set_allocated_p2_sigs(groupedSigs);
        proof->set_p2_view(view);
        //view = groupedSigs.begin()->second.sigs().begin()
      }
    }
  }

  for (const auto &read : txn->read_set()) {
    if (!IsKeyOwned(read.key())) {
      continue;
    }
     //if(params.mainThreadDispatching) storeMutex.lock(); //probably dont need? --> add locks to store.

    //store.commitGet(read.key(), read.readtime(), ts);   //SEEMINGLY NEVER USED XXX
    //commitGet_count++;
     //if(params.mainThreadDispatching) storeMutex.unlock();
    //Latency_Start(&committedReadInsertLat);
    //Latency_Start(&waitingOnLocks);
     //if(params.mainThreadDispatching) committedReadsMutex.lock();
     //Latency_End(&waitingOnLocks);
     //this could be a readlock for dstruct + a lock per object. (need write lock the first time)

     std::pair<std::shared_mutex, std::set<committedRead>> &z = committedReads[read.key()];
     std::unique_lock lock(z.first);
     z.second.insert(std::make_tuple(ts, read.readtime(),
           committedItr.first->second));

    // committedReads[read.key()].insert(std::make_tuple(ts, read.readtime(),
    //       committedItr.first->second));
     //if(params.mainThreadDispatching) committedReadsMutex.unlock();
    //uint64_t ns = Latency_End(&committedReadInsertLat);
    //stats.Add("committed_read_insert_lat_" + BytesToHex(read.key(), 18), ns);
  }


  for (const auto &write : txn->write_set()) {
    if (!IsKeyOwned(write.key())) {
      continue;
    }

    Debug("COMMIT[%lu,%lu] Committing write for key %s.",
        txn->client_id(), txn->client_seq_num(),
        BytesToHex(write.key(), 16).c_str());
    val.val = write.value();
     //if(params.mainThreadDispatching) storeMutex.lock();
    store.put(write.key(), val, ts);
     //if(params.mainThreadDispatching) storeMutex.unlock();


     //RECOMMENT
    //  //Latency_Start(&waitingOnLocks);
    //  if(params.mainThreadDispatching) rtsMutex.lock();
    //  //Latency_End(&waitingOnLocks);
    // auto rtsItr = rts.find(write.key());
    // if (rtsItr != rts.end()) {
    //   auto itr = rtsItr->second.begin();
    //   auto endItr = rtsItr->second.upper_bound(ts);
    //   while (itr != endItr) {
    //     itr = rtsItr->second.erase(itr);
    //   }
    // }
    //  if(params.mainThreadDispatching) rtsMutex.unlock();
  }

  Clean(txnDigest);
  CheckDependents(txnDigest);
  CleanDependencies(txnDigest);
}

void Server::Abort(const std::string &txnDigest) {
   //if(params.mainThreadDispatching) abortedMutex.lock();
  aborted.insert(txnDigest);
   //if(params.mainThreadDispatching) abortedMutex.unlock();
  Clean(txnDigest);
  CheckDependents(txnDigest);
  CleanDependencies(txnDigest);
}

void Server::Clean(const std::string &txnDigest) {

  //Latency_Start(&waitingOnLocks);
  //auto ongoingMutexScope = params.mainThreadDispatching ? std::unique_lock<std::shared_mutex>(ongoingMutex) : std::unique_lock<std::shared_mutex>();
  //auto preparedMutexScope = params.mainThreadDispatching ? std::unique_lock<std::shared_mutex>(preparedMutex) : std::unique_lock<std::shared_mutex>();
  //auto preparedReadsMutexScope = params.mainThreadDispatching ? std::unique_lock<std::shared_mutex>(preparedReadsMutex) : std::unique_lock<std::shared_mutex>();
  //auto preparedWritesMutexScope = params.mainThreadDispatching ? std::unique_lock<std::shared_mutex>(preparedWritesMutex) : std::unique_lock<std::shared_mutex>();

  //auto p1ConflictsMutexScope = params.mainThreadDispatching ? std::unique_lock<std::mutex>(p1ConflictsMutex) : std::unique_lock<std::mutex>();
  //p1Decisions..
  auto p2DecisionsMutexScope = params.mainThreadDispatching ? std::unique_lock<std::mutex>(p2DecisionsMutex) : std::unique_lock<std::mutex>();
  //auto interestedClientsMutexScope = params.mainThreadDispatching ? std::unique_lock<std::mutex>(interestedClientsMutex) : std::unique_lock<std::mutex>();
  auto current_viewsMutexScope = params.mainThreadDispatching ? std::unique_lock<std::mutex>(current_viewsMutex) : std::unique_lock<std::mutex>();
  auto decision_viewsMutexScope = params.mainThreadDispatching ? std::unique_lock<std::mutex>(decision_viewsMutex) : std::unique_lock<std::mutex>();
  auto ElectQuorumMutexScope = params.mainThreadDispatching ? std::unique_lock<std::mutex>(ElectQuorumMutex) : std::unique_lock<std::mutex>();
  //Latency_End(&waitingOnLocks);


  preparedMap::accessor a;

  auto preparedItr = prepared.find(a, txnDigest);
  if(preparedItr){
  //if (itr != prepared.end()) {
    for (const auto &read : a->second.second->read_set()) {
    //for (const auto &read : itr->second.second->read_set()) {
      if (IsKeyOwned(read.key())) {
        //preparedReads[read.key()].erase(a->second.second);
        //preparedReads[read.key()].erase(itr->second.second);
        std::pair<std::shared_mutex, std::set<const proto::Transaction *>> &y = preparedReads[read.key()];
        std::unique_lock lock(y.first);
        y.second.erase(a->second.second);

      }
    }
    for (const auto &write : a->second.second->write_set()) {
    //for (const auto &write : itr->second.second->write_set()) {
      if (IsKeyOwned(write.key())) {
        //preparedWrites[write.key()].erase(itr->second.first);
        std::pair<std::shared_mutex,std::map<Timestamp, const proto::Transaction *>> &x = preparedWrites[write.key()];
        std::unique_lock lock(x.first);
        x.second.erase(a->second.first);
        //x.second.erase(itr->second.first);
      }
    }
    prepared.erase(a);
  }
  a.release();
  ongoingMap::accessor b;
  ongoing.find(b, txnDigest);
  ongoing.erase(b);
  //ongoing.erase(txnDigest);
  b.release();

  //Fallback related cleans

  //interestedClients[txnDigest].insert(remote.clone());
  interestedClientsMap::accessor i;
  auto jtr = interestedClients.find(i, txnDigest);
  if(jtr){
  //if (jtr != interestedClients.end()) {
    for (const auto addr : i->second) {
      delete addr;
    }
    interestedClients.erase(i);
  }
  i.release();

  current_views.erase(txnDigest);
  decision_views.erase(txnDigest);
  auto ktr = ElectQuorum.find(txnDigest);
  if (ktr != ElectQuorum.end()) {
    for (const auto signed_m : ktr->second) {
      delete signed_m;
    }
    ElectQuorum.erase(ktr);
    ElectQuorum_meta.erase(txnDigest);
  }
  p1DecisionsMap::accessor c;
  auto p1DecItr = p1Decisions.find(c, txnDigest);
  //p1Decisions.erase(txnDigest);
  if(p1DecItr) p1Decisions.erase(c);
  c.release();

  p1ConflictsMap::accessor d;
  auto p1ConfItr = p1Conflicts.find(d, txnDigest);
  //p1Conflicts.erase(txnDigest);
  if(p1ConfItr) p1Conflicts.erase(d);
  d.release();

  p2Decisions.erase(txnDigest);
  //TODO: erase all timers if we use them again
}

void Server::CheckDependents(const std::string &txnDigest) {
  //Latency_Start(&waitingOnLocks);
   if(params.mainThreadDispatching) dependentsMutex.lock(); //read lock
   if(params.mainThreadDispatching) waitingDependenciesMutex.lock();
  //Latency_End(&waitingOnLocks);

  std::cout << "ABORTING AT 2" << std::endl;
  dependentsMap::const_accessor e;
  auto dependentsItr = dependents.find(e, txnDigest);
  if(dependentsItr){
  //if (dependentsItr != dependents.end()) {
    for (const auto &dependent : e->second) {
    //for (const auto &dependent : dependentsItr->second) {
      auto dependenciesItr = waitingDependencies.find(dependent);
      UW_ASSERT(dependenciesItr != waitingDependencies.end());

      dependenciesItr->second.deps.erase(txnDigest);
      if (dependenciesItr->second.deps.size() == 0) {
        Debug("Dependencies of %s have all committed or aborted.",
            BytesToHex(dependent, 16).c_str());
        proto::ConcurrencyControl::Result result = CheckDependencies(
            dependent);
        UW_ASSERT(result != proto::ConcurrencyControl::ABORT);
        Debug("print remote: %p", dependenciesItr->second.remote);
        //waitingDependencies.erase(dependent);
        const proto::CommittedProof *conflict = nullptr;
        SendPhase1Reply(dependenciesItr->second.reqId, result, conflict, dependent,
            dependenciesItr->second.remote);
        delete dependenciesItr->second.remote;
        waitingDependencies.erase(dependent);
      }
    }
  }
  e.release();
   if(params.mainThreadDispatching) dependentsMutex.unlock();
   if(params.mainThreadDispatching) waitingDependenciesMutex.unlock();
}

proto::ConcurrencyControl::Result Server::CheckDependencies(
    const std::string &txnDigest) {
      //Latency_Start(&waitingOnLocks);
   //if(params.mainThreadDispatching) ongoingMutex.lock_shared();
   //Latency_End(&waitingOnLocks);
  ongoingMap::const_accessor b;
  auto txnItr = ongoing.find(b, txnDigest);
  if(!txnItr){

  //if(txnItr == ongoing.end()){
    Debug("Tx with txn digest [%s] has already committed/aborted", BytesToHex(txnDigest, 16).c_str());
    {
      //std::shared_lock lock(committedMutex);
      //std::shared_lock lock2(abortedMutex);
      if(committed.find(txnDigest) != committed.end()){
        //if(params.mainThreadDispatching) ongoingMutex.unlock_shared();
        return proto::ConcurrencyControl::COMMIT;
      }
      else if(aborted.find(txnDigest) != aborted.end()){
        //if(params.mainThreadDispatching) ongoingMutex.unlock_shared();
        return proto::ConcurrencyControl::ABSTAIN;
      }
      else{
        Panic("has to be either committed or aborted");
      }
    }
  }

  //UW_ASSERT(txnItr != ongoing.end());
   //if(params.mainThreadDispatching) ongoingMutex.unlock_shared();
  //return CheckDependencies(*txnItr->second);
  return CheckDependencies(*b->second);
}

proto::ConcurrencyControl::Result Server::CheckDependencies(
    const proto::Transaction &txn) {

   //if(params.mainThreadDispatching) committedMutex.lock_shared();
  for (const auto &dep : txn.deps()) {
    if (dep.involved_group() != groupIdx) {
      continue;
    }
    if (committed.find(dep.write().prepared_txn_digest()) != committed.end()) {
      if (Timestamp(dep.write().prepared_timestamp()) > Timestamp(txn.timestamp())) {
        stats.Increment("cc_aborts", 1);
        stats.Increment("cc_aborts_dep_ts", 1);
         //if(params.mainThreadDispatching) committedMutex.unlock_shared();
        return proto::ConcurrencyControl::ABSTAIN;
      }
    } else {
      stats.Increment("cc_aborts", 1);
      stats.Increment("cc_aborts_dep_aborted", 1);
       //if(params.mainThreadDispatching) committedMutex.unlock_shared();
      return proto::ConcurrencyControl::ABSTAIN;
    }
  }
   //if(params.mainThreadDispatching) committedMutex.unlock_shared();
  return proto::ConcurrencyControl::COMMIT;
}

bool Server::CheckHighWatermark(const Timestamp &ts) {
  Timestamp highWatermark(timeServer.GetTime());
  // add delta to current local time
  highWatermark.setTimestamp(highWatermark.getTimestamp() + timeDelta);
  Debug("High watermark: %lu.", highWatermark.getTimestamp());
  return ts > highWatermark;
}

void Server::SendPhase1Reply(uint64_t reqId,
    proto::ConcurrencyControl::Result result,
    const proto::CommittedProof *conflict, const std::string &txnDigest,
    const TransportAddress *remote) {
   //if(params.mainThreadDispatching) p1DecisionsMutex.lock();
   p1DecisionsMap::accessor c;
   p1Decisions.insert(c, std::make_pair(txnDigest, result));
   c.release();
 //  p1Decisions[txnDigest] = result;
   //if(params.mainThreadDispatching) p1DecisionsMutex.unlock();
  //add abort proof so we can use it for fallbacks easily.
  if(result == proto::ConcurrencyControl::ABORT){
     //if(params.mainThreadDispatching) p1ConflictsMutex.lock();
     p1ConflictsMap::accessor d;
     p1Conflicts.insert(d, std::make_pair(txnDigest, conflict));
    //p1Conflicts[txnDigest] = conflict;  //does this work this way for CbR
    d.release();
     //if(params.mainThreadDispatching) p1ConflictsMutex.unlock();
  }

  proto::Phase1Reply* phase1Reply = GetUnusedPhase1Reply();
  phase1Reply->set_req_id(reqId);
  TransportAddress *remoteCopy = remote->clone();

  auto sendCB = [remoteCopy, this, phase1Reply]() {
    //std::unique_lock<std::mutex> lock(transportMutex);

    this->transport->SendMessage(this, *remoteCopy, *phase1Reply);
    FreePhase1Reply(phase1Reply);
    delete remoteCopy;
  };

  // auto sendCB_wrapped = [remoteCopy, this, phase1Reply](void* arg) {
  //   //std::unique_lock<std::mutex> lock(transportMutex);
  //
  //   this->transport->SendMessage(this, *remoteCopy, *phase1Reply);
  //   FreePhase1Reply(phase1Reply);
  //   delete remoteCopy;
  // };
  // auto sendCB = [this, scb = std::move(sendCB_wrapped)]() {
  //   //std::unique_lock<std::mutex> lock(transportMutex);
  //   this->transport->IssueCB(scb, (void*) true);
  // };

  phase1Reply->mutable_cc()->set_ccr(result);
  if (params.validateProofs) {
    *phase1Reply->mutable_cc()->mutable_txn_digest() = txnDigest;
    phase1Reply->mutable_cc()->set_involved_group(groupIdx);
    if (result == proto::ConcurrencyControl::ABORT) {
      *phase1Reply->mutable_cc()->mutable_committed_conflict() = *conflict;
    } else if (params.signedMessages) {
      proto::ConcurrencyControl* cc = new proto::ConcurrencyControl(phase1Reply->cc());
      //Latency_Start(&signLat);
      Debug("PHASE1[%s] Batching Phase1Reply.",
            BytesToHex(txnDigest, 16).c_str());

      MessageToSign(cc, phase1Reply->mutable_signed_cc(),
        [sendCB, cc, txnDigest, this, phase1Reply]() {
          Debug("PHASE1[%s] Sending Phase1Reply with signature %s from priv key %lu.",
            BytesToHex(txnDigest, 16).c_str(),
            BytesToHex(phase1Reply->signed_cc().signature(), 100).c_str(),
            phase1Reply->signed_cc().process_id());
          //sanity checks
          // Debug("P1 SIGNATURE TX:[%s] with Sig:[%s] from replica %lu with Msg:[%s].",
          //   BytesToHex(txnDigest, 16).c_str(),
          //   BytesToHex(phase1Reply->signed_cc().signature(), 1024).c_str(),
          //   phase1Reply->signed_cc().process_id(),
          //   BytesToHex(phase1Reply->signed_cc().data(), 1024).c_str());

          sendCB();
          delete cc;
        });
      //Latency_End(&signLat);
      return;
    }
  }

  sendCB();
}

void Server::CleanDependencies(const std::string &txnDigest) {
   if(params.mainThreadDispatching) dependentsMutex.lock();
   if(params.mainThreadDispatching) waitingDependenciesMutex.lock();

  auto dependenciesItr = waitingDependencies.find(txnDigest);
  if (dependenciesItr != waitingDependencies.end()) {
    for (const auto &dependency : dependenciesItr->second.deps) {
      std::cout << "ABORTING AT 3" << std::endl;
      dependentsMap::accessor e;
      auto dependentItr = dependents.find(e, dependency);
      // if (dependentItr != dependents.end()) {
      //   dependentItr->second.erase(txnDigest);
      // }
      if (dependentItr) {
        e->second.erase(txnDigest);
      }
      e.release();
    }
    waitingDependencies.erase(dependenciesItr);
  }
  std::cout << "ABORTING AT 4" << std::endl;
  dependentsMap::accessor e;
  dependents.find(e, txnDigest);
  dependents.erase(e);
  e.release();
  //dependents.erase(txnDigest);

   if(params.mainThreadDispatching) dependentsMutex.unlock();
   if(params.mainThreadDispatching) waitingDependenciesMutex.unlock();
}

void Server::LookupP1Decision(const std::string &txnDigest, int64_t &myProcessId,
    proto::ConcurrencyControl::Result &myResult) const {
  myProcessId = -1;
  // see if we participated in this decision
   //if(params.mainThreadDispatching) p1DecisionsMutex.lock();
   p1DecisionsMap::const_accessor c;

  auto p1DecisionItr = p1Decisions.find(c, txnDigest);
  if(p1DecisionItr){
  //if (p1DecisionItr != p1Decisions.end()) {
    myProcessId = id;
    myResult = c->second;
  }
  c.release();
   //if(params.mainThreadDispatching) p1DecisionsMutex.unlock();
}

void Server::LookupP2Decision(const std::string &txnDigest, int64_t &myProcessId,
    proto::CommitDecision &myDecision) const {
  myProcessId = -1;
  // see if we participated in this decision
   if(params.mainThreadDispatching) p2DecisionsMutex.lock();
  auto p2DecisionItr = p2Decisions.find(txnDigest);
  if (p2DecisionItr != p2Decisions.end()) {
    myProcessId = id;
    myDecision = p2DecisionItr->second;
  }
   if(params.mainThreadDispatching) p2DecisionsMutex.unlock();
}

uint64_t Server::DependencyDepth(const proto::Transaction *txn) const {
  uint64_t maxDepth = 0;
  std::queue<std::pair<const proto::Transaction *, uint64_t>> q;
  q.push(std::make_pair(txn, 0UL));

  //auto ongoingMutexScope = params.mainThreadDispatching ? std::shared_lock<std::shared_mutex>(ongoingMutex) : std::shared_lock<std::shared_mutex>();

  while (!q.empty()) {
    std::pair<const proto::Transaction *, uint64_t> curr = q.front();
    q.pop();
    maxDepth = std::max(maxDepth, curr.second);
    for (const auto &dep : curr.first->deps()) {
      ongoingMap::const_accessor b;
      auto oitr = ongoing.find(b, dep.write().prepared_txn_digest());
      if(oitr){
      //if (oitr != ongoing.end()) {
        //q.push(std::make_pair(oitr->second, curr.second + 1));
        q.push(std::make_pair(b->second, curr.second + 1));
      }
      b.release();
    }
  }
  return maxDepth;
}

void Server::MessageToSign(::google::protobuf::Message* msg,
      proto::SignedMessage *signedMessage, signedCallback cb) {


  ////auto lockScope = params.mainThreadDispatching ? std::unique_lock<std::mutex>(mainThreadMutex) : std::unique_lock<std::mutex>();
  Debug("Exec MessageToSign by CPU: %d", sched_getcpu());

  if(params.multiThreading){
      if (params.signatureBatchSize == 1) {
          // ::google::protobuf::Message* msg_copy = msg.New();
          // msg_copy->CopyFrom(msg);
          // std::function<void*()> f(std::bind(asyncSignMessage, *msg_copy, keyManager->GetPrivateKey(id), id, signedMessage));
          //Assuming bind creates a copy this suffices:
          Debug("(multithreading) dispatching signing");
      // std::function<void*()> f(std::bind(asyncSignMessage, msg, keyManager->GetPrivateKey(id),
      //  id, signedMessage));
      // transport->DispatchTP(std::move(f), [cb](void * ret){ cb();});

      auto f = [this, msg, signedMessage, cb = std::move(cb)](){
        SignMessage(msg, keyManager->GetPrivateKey(id), id, signedMessage);
        cb();
        return (void*) true;
      };
      transport->DispatchTP_noCB(std::move(f));
      }

      else {
        Debug("(multithreading) adding sig request to localbatchSigner");
        // auto f = [this, msg, signedMessage, cb = std::move(cb)](){
        //   //std::unique_lock<std::mutex> lock(this->batchSigner->batchMutex);
        //   this->batchSigner->asyncMessageToSign(msg, signedMessage, std::move(cb));
        //   return (void*) true;
        // };
        // transport->DispatchTP_noCB(std::move(f));
        //batchSigner->MessageToSign(msg, signedMessage, std::move(cb));

        batchSigner->asyncMessageToSign(msg, signedMessage, std::move(cb));
      }
  }
  else{
    if (params.signatureBatchSize == 1) {
      // std::string str;
      // msg->SerializeToString(&str);
      // Debug("message: %s", BytesToHex(str, 128).c_str());
      SignMessage(msg, keyManager->GetPrivateKey(id), id, signedMessage);
      cb();
      //if multithread: Dispatch f: SignMessage and cb.
    } else {
      batchSigner->MessageToSign(msg, signedMessage, std::move(cb));
    }
  }
}


proto::ReadReply *Server::GetUnusedReadReply() {
  return new proto::ReadReply();

  // std::unique_lock<std::mutex> lock(readReplyProtoMutex);
  // proto::ReadReply *reply;
  // if (readReplies.size() > 0) {
  //   reply = readReplies.back();
  //   reply->Clear();
  //   readReplies.pop_back();
  // } else {
  //   reply = new proto::ReadReply();
  // }
  // return reply;
}

proto::Phase1Reply *Server::GetUnusedPhase1Reply() {
  return new proto::Phase1Reply();

  // //Latency_Start(&waitOnProtoLock);
  // std::unique_lock<std::mutex> lock(p1ReplyProtoMutex);
  // //Latency_End(&waitOnProtoLock);
  // proto::Phase1Reply *reply;
  // if (p1Replies.size() > 0) {
  //   reply = p1Replies.back();
  //   //reply->Clear(); //can move this to Free if want more work at threads
  //   p1Replies.pop_back();
  // } else {
  //   reply = new proto::Phase1Reply();
  // }
  // return reply;
}

proto::Phase2Reply *Server::GetUnusedPhase2Reply() {
  return new proto::Phase2Reply();

  // //Latency_Start(&waitOnProtoLock);
  // std::unique_lock<std::mutex> lock(p2ReplyProtoMutex);
  // //Latency_End(&waitOnProtoLock);
  // proto::Phase2Reply *reply;
  // if (p2Replies.size() > 0) {
  //   reply = p2Replies.back();
  //   //reply->Clear();
  //   p2Replies.pop_back();
  // } else {
  //   reply = new proto::Phase2Reply();
  // }
  // return reply;
}

proto::Read *Server::GetUnusedReadmessage() {
  return new proto::Read();

  // std::unique_lock<std::mutex> lock(readProtoMutex);
  // proto::Read *msg;
  // if (readMessages.size() > 0) {
  //   msg = readMessages.back();
  //   msg->Clear();
  //   readMessages.pop_back();
  // } else {
  //   msg = new proto::Read();
  // }
  // return msg;
}

proto::Phase1 *Server::GetUnusedPhase1message() {
  return new proto::Phase1();

  // //Latency_Start(&waitOnProtoLock);
  // std::unique_lock<std::mutex> lock(p1ProtoMutex);
  // //Latency_End(&waitOnProtoLock);
  // proto::Phase1 *msg;
  // if (p1messages.size() > 0) {
  //   msg = p1messages.back();
  //   msg->Clear();
  //   p1messages.pop_back();
  // } else {
  //   msg = new proto::Phase1();
  // }
  // return msg;
}

proto::Phase2 *Server::GetUnusedPhase2message() {
  return new proto::Phase2();

  // //Latency_Start(&waitOnProtoLock);
  // std::unique_lock<std::mutex> lock(p2ProtoMutex);
  // //Latency_End(&waitOnProtoLock);
  // proto::Phase2 *msg;
  // if (p2messages.size() > 0) {
  //   msg = p2messages.back();
  //   msg->Clear();
  //   p2messages.pop_back();
  // } else {
  //   msg = new proto::Phase2();
  // }
  // return msg;
}

proto::Writeback *Server::GetUnusedWBmessage() {
  return new proto::Writeback();

  // //Latency_Start(&waitOnProtoLock);
  // std::unique_lock<std::mutex> lock(WBProtoMutex);
  // //Latency_End(&waitOnProtoLock);
  // proto::Writeback *msg;
  // if (WBmessages.size() > 0) {
  //   msg = WBmessages.back();
  //   msg->Clear();
  //   WBmessages.pop_back();
  // } else {
  //   msg = new proto::Writeback();
  // }
  // return msg;
}

void Server::FreeReadReply(proto::ReadReply *reply) {
  delete reply;
  // std::unique_lock<std::mutex> lock(readReplyProtoMutex);
  // //reply->Clear();
  // readReplies.push_back(reply);
}

void Server::FreePhase1Reply(proto::Phase1Reply *reply) {
  delete reply;
  // std::unique_lock<std::mutex> lock(p1ReplyProtoMutex);
  //
  // reply->Clear();
  // p1Replies.push_back(reply);
}

void Server::FreePhase2Reply(proto::Phase2Reply *reply) {
  delete reply;
  // std::unique_lock<std::mutex> lock(p2ReplyProtoMutex);
  // reply->Clear();
  // p2Replies.push_back(reply);
}

void Server::FreeReadmessage(proto::Read *msg) {
  delete msg;
  // std::unique_lock<std::mutex> lock(readProtoMutex);
  // readMessages.push_back(msg);
}

void Server::FreePhase1message(proto::Phase1 *msg) {
  delete msg;
  // //Latency_Start(&waitOnProtoLock);
  // std::unique_lock<std::mutex> lock(p1ProtoMutex);
  // //Latency_End(&waitOnProtoLock);
  // p1messages.push_back(msg);
}

void Server::FreePhase2message(proto::Phase2 *msg) {
  delete msg;
  // //Latency_Start(&waitOnProtoLock);
  // std::unique_lock<std::mutex> lock(p2ProtoMutex);
  // //Latency_End(&waitOnProtoLock);
  // p2messages.push_back(msg);
}

void Server::FreeWBmessage(proto::Writeback *msg) {
  delete msg;
  // //Latency_Start(&waitOnProtoLock);
  // std::unique_lock<std::mutex> lock(WBProtoMutex);
  // //Latency_End(&waitOnProtoLock);
  // WBmessages.push_back(msg);
}

////////////////////// Fallback realm beings here... enter at own risk

//Simulated HMAC code
std::unordered_map<uint64_t, std::string> sessionKeys;
void CreateSessionKeys();
bool ValidateHMACedMessage(const proto::SignedMessage &signedMessage);
void CreateHMACedMessage(const ::google::protobuf::Message &msg, proto::SignedMessage& signedMessage);
// assume these are somehow secretly shared before hand


void Server::CreateSessionKeys(){
  for (uint64_t i = 0; i < config.n; i++) {
    if (i > idx) {
      sessionKeys[i] = std::string(8, (char) idx + 0x30) + std::string(8, (char) i + 0x30);
    } else {
      sessionKeys[i] = std::string(8, (char) i + 0x30) + std::string(8, (char) idx + 0x30);
    }
  }
}

// create MAC messages and verify them: Used for all to all leader election.
bool Server::ValidateHMACedMessage(const proto::SignedMessage &signedMessage) {

proto::HMACs hmacs;
hmacs.ParseFromString(signedMessage.signature());
return crypto::verifyHMAC(signedMessage.data(), (*hmacs.mutable_hmacs())[idx], sessionKeys[signedMessage.process_id() % config.n]);
}

void Server::CreateHMACedMessage(const ::google::protobuf::Message &msg, proto::SignedMessage& signedMessage) {

std::string msgData = msg.SerializeAsString();
signedMessage.set_data(msgData);
signedMessage.set_process_id(id);

proto::HMACs hmacs;
for (uint64_t i = 0; i < config.n; i++) {
  (*hmacs.mutable_hmacs())[i] = crypto::HMAC(msgData, sessionKeys[i]);
}
signedMessage.set_signature(hmacs.SerializeAsString());
}


/////////



//TODO: all requestID entries can be deleted..
void Server::HandlePhase1FB(const TransportAddress &remote, proto::Phase1FB &msg) {

  std::string txnDigest = TransactionDigest(msg.txn(), params.hashDigest);
  Debug("PHASE1FB[%lu:%lu][%s] with ts %lu.", msg.txn().client_id(),
      msg.txn().client_seq_num(), BytesToHex(txnDigest, 16).c_str(),
      msg.txn().timestamp().timestamp());

//check if already committed. reply with whole proof so client can forward that.     committed aborted
    //if(committed.end() != committed.find(txnDigest) )) {
    //  proto::CommittedProof* proof = committed[txnDigest];
    //}
    //else if(aborted.end() != aborted.find(txnDigest) ){
    //}

    //KEEP track of interested client
    interestedClientsMap::accessor i;
    bool interestedClientsItr = interestedClients.insert(i, txnDigest);
    i->second.insert(remote.clone());
    i.release();
    //interestedClients[txnDigest].insert(remote.clone());


//currently for simplicity just forward writeback message that we received and stored.
    //ABORT CASE
       //if(params.mainThreadDispatching) committedMutex.lock();
       //if(params.mainThreadDispatching) p1DecisionsMutex.lock();

       p1DecisionsMap::const_accessor c;
       auto p1DecItr = p1Decisions.find(c, txnDigest);
       if(params.mainThreadDispatching) p2DecisionsMutex.lock();
       if(params.mainThreadDispatching) writebackMessagesMutex.lock();

    if(writebackMessages.find(txnDigest) != writebackMessages.end()){
      writeback = writebackMessages[txnDigest];

         //if(params.mainThreadDispatching) committedMutex.unlock();
         //if(params.mainThreadDispatching) p1DecisionsMutex.unlock();
         if(params.mainThreadDispatching) p2DecisionsMutex.unlock();
         if(params.mainThreadDispatching) writebackMessagesMutex.unlock();

      SendPhase1FBReply(msg.req_id(), phase1Reply, phase2Reply, writeback, remote,  txnDigest, 1);
      return;
    }
    //COMMIT CASE
    else if(committed.find(txnDigest) != committed.end()){
      writeback.Clear();
      writeback.set_decision(proto::COMMIT);
      writeback.set_txn_digest(txnDigest);

      if(committed[txnDigest]->has_p1_sigs()){
        *writeback.mutable_p1_sigs() = committed[txnDigest]->p1_sigs();
      }
      else if(committed[txnDigest]->has_p2_sigs()){
        *writeback.mutable_p2_sigs() = committed[txnDigest]->p2_sigs();
      }
      else{
         //if(params.mainThreadDispatching) committedMutex.unlock();
         //if(params.mainThreadDispatching) p1DecisionsMutex.unlock();
         if(params.mainThreadDispatching) p2DecisionsMutex.unlock();
         if(params.mainThreadDispatching) writebackMessagesMutex.unlock();
        return; //error
      }
       //if(params.mainThreadDispatching) committedMutex.unlock();
       //if(params.mainThreadDispatching) p1DecisionsMutex.unlock();
       if(params.mainThreadDispatching) p2DecisionsMutex.unlock();
       if(params.mainThreadDispatching) writebackMessagesMutex.unlock();
      SendPhase1FBReply(msg.req_id(), phase1Reply, phase2Reply, writeback, remote,  txnDigest, 1);
      return;
    }
    //add case where there is both p2 and p1
    else if(p2Decisions.end() != p2Decisions.find(txnDigest) && p1DecItr){
    //else if(p2Decisions.end() != p2Decisions.find(txnDigest) && p1Decisions.end() != p1Decisions.find(txnDigest) ){
       proto::CommitDecision decision = p2Decisions[txnDigest];    //might want to include the p1 too in order for there to exist a quorum for p1r (if not enough p2r). if you dont have a p1, then execute it yourself. Alternatively, keep around the decision proof and send it. For now/simplicity, p2 suffices
       //proto::ConcurrencyControl::Result result = p1Decisions[txnDigest];
       proto::ConcurrencyControl::Result result = c->second;
        //if(params.mainThreadDispatching) committedMutex.unlock();
        //if(params.mainThreadDispatching) p1DecisionsMutex.unlock();
        if(params.mainThreadDispatching) p2DecisionsMutex.unlock();
        if(params.mainThreadDispatching) writebackMessagesMutex.unlock();

       proto::CommittedProof conflict;
       //recover stored commit proof.
       if (result != proto::ConcurrencyControl::WAIT) {
         const proto::CommittedProof *conflict;
         //recover stored commit proof.
         if(result == proto::ConcurrencyControl::ABORT){
            //if(params.mainThreadDispatching) p1ConflictsMutex.lock();
            p1ConflictsMap::const_accessor d;
            auto p1ConflictsItr = p1Conflicts.find(d, txnDigest);
            conflict = d->second;
            d.release();
            //conflict = p1Conflicts[txnDigest];
            //if(params.mainThreadDispatching) p1ConflictsMutex.unlock();
         }
         SetP1(msg.req_id(), txnDigest, result, conflict);
         SendPhase1FBReply(msg.req_id(), phase1Reply, phase2Reply, writeback, remote,  txnDigest, 4);
       }
       SetP2(msg.req_id(), txnDigest, decision);
       SendPhase1FBReply(msg.req_id(), phase1Reply, phase2Reply, writeback, remote,   txnDigest, 2);
    }

//  Check whether already did p2, but was not part of p1
    else if(p2Decisions.end() != p2Decisions.find(txnDigest)){
       proto::CommitDecision decision = p2Decisions[txnDigest];

        //if(params.mainThreadDispatching) committedMutex.unlock();
        //if(params.mainThreadDispatching) p1DecisionsMutex.unlock();
        if(params.mainThreadDispatching) p2DecisionsMutex.unlock();
        if(params.mainThreadDispatching) writebackMessagesMutex.unlock();

       SetP2(msg.req_id(), txnDigest, decision);
       SendPhase1FBReply(msg.req_id(), phase1Reply, phase2Reply, writeback, remote,  txnDigest, 3);
    }
    //Else if: Check whether already did p1 but no p2     p1Decisions
    else if(p1DecItr){
  //  else if(p1Decisions.end() != p1Decisions.find(txnDigest)){
        //proto::ConcurrencyControl::Result result = p1Decisions[txnDigest];
        proto::ConcurrencyControl::Result result = c->second;

         //if(params.mainThreadDispatching) committedMutex.unlock();
         //if(params.mainThreadDispatching) p1DecisionsMutex.unlock();
         if(params.mainThreadDispatching) p2DecisionsMutex.unlock();
         if(params.mainThreadDispatching) writebackMessagesMutex.unlock();

        if (result != proto::ConcurrencyControl::WAIT) {
          const proto::CommittedProof *conflict;
          //recover stored commit proof.
          if(result == proto::ConcurrencyControl::ABORT){
             //if(params.mainThreadDispatching) p1ConflictsMutex.lock();
             p1ConflictsMap::const_accessor d;
             auto p1ConflictsItr = p1Conflicts.find(d, txnDigest);
             //conflict = p1Conflicts[txnDigest];
             conflict = d->second;
             d.release();
             //if(params.mainThreadDispatching) p1ConflictsMutex.unlock();
          }
          SetP1(msg.req_id(), txnDigest, result, conflict);
          SendPhase1FBReply(msg.req_id(), phase1Reply, phase2Reply, writeback, remote,  txnDigest, 4);
        }
    }
    //Else Do p1 normally. copied logic from HandlePhase1(remote, msg)
    else{
       //if(params.mainThreadDispatching) committedMutex.unlock();
       //if(params.mainThreadDispatching) p1DecisionsMutex.unlock();
       if(params.mainThreadDispatching) p2DecisionsMutex.unlock();
       if(params.mainThreadDispatching) writebackMessagesMutex.unlock();

      std::string txnDigest = TransactionDigest(msg.txn(), params.hashDigest);
      Debug("FB exec PHASE1[%lu:%lu][%s] with ts %lu.", msg.txn().client_id(),
          msg.txn().client_seq_num(), BytesToHex(txnDigest, 16).c_str(),
          msg.txn().timestamp().timestamp());

      if (params.validateProofs && params.signedMessages && params.verifyDeps) {
        for (const auto &dep : msg.txn().deps()) {
          if (!dep.has_write_sigs()) {
            Debug("Dep for txn %s missing signatures.",
              BytesToHex(txnDigest, 16).c_str());
            return;
          }
          if (!ValidateDependency(dep, &config, params.readDepSize, keyManager,
                verifier)) {
            Debug("VALIDATE Dependency failed for txn %s.",
                BytesToHex(txnDigest, 16).c_str());
            // safe to ignore Byzantine client
            return;
          }
        }
      }
      c.release();
      //start new current view
      current_views[txnDigest] = 0;

      proto::Transaction *txn = msg.release_txn();
       ////if(params.mainThreadDispatching) ongoingMutex.lock();
      ongoingMap::accessor b;
      ongoing.insert(b, std::make_pair(txnDigest, txn));
      //ongoing[txnDigest] = txn;
      b.release();
       ////if(params.mainThreadDispatching) ongoingMutex.unlock();

      Timestamp retryTs;
      const proto::CommittedProof *committedProof;
      proto::ConcurrencyControl::Result result = DoOCCCheck(msg.req_id(),
          remote, txnDigest, *txn, retryTs, committedProof);

       //if(params.mainThreadDispatching) p1DecisionsMutex.lock();
      p1DecisionsMap::accessor c2;
      p1Decisions.insert(c, std::make_pair(txnDigest, result));
      //p1Decisions[txnDigest] = result;
      c2.release();
       //if(params.mainThreadDispatching) p1DecisionsMutex.unlock();
      if(result == proto::ConcurrencyControl::ABORT){
         //if(params.mainThreadDispatching) p1ConflictsMutex.lock();
         p1ConflictsMap::accessor d;
         p1Conflicts.insert(d, std::make_pair(txnDigest, committedProof));
         //p1Conflicts[txnDigest] = committedProof;  //does this work this way for CbR
         d.release();
         //if(params.mainThreadDispatching) p1ConflictsMutex.unlock();
      }

    //TODO: WHen will it be sent if its WAIT?  --> will be in some notify function. NEED TO EDIT THAT TO SEND TO ALL INTERESTED CLIENTS!!!
      if (result != proto::ConcurrencyControl::WAIT) {
            SetP1(msg.req_id(), txnDigest, result, committedProof);
            if(client_starttime.find(txnDigest) == client_starttime.end()){
              struct timeval tv;
              gettimeofday(&tv, NULL);
              uint64_t start_time = (tv.tv_sec*1000000+tv.tv_usec)/1000;  //in miliseconds
              client_starttime[txnDigest] = start_time;
            }
          SendPhase1FBReply(msg.req_id(), phase1Reply, phase2Reply, writeback, remote,  txnDigest, 4);

      }    //want to sendPhase1FBReply with the content from HandlePhase1.
  }
}

void Server::SetP1(uint64_t reqId, std::string txnDigest, proto::ConcurrencyControl::Result &result, const proto::CommittedProof *conflict){
  phase1Reply.Clear();
  phase1Reply.set_req_id(reqId);
  phase1Reply.mutable_cc()->set_ccr(result);
  if (params.validateProofs) {
    *phase1Reply.mutable_cc()->mutable_txn_digest() = txnDigest;
    if (result == proto::ConcurrencyControl::ABORT) {
      *phase1Reply.mutable_cc()->mutable_committed_conflict() = *conflict;
    } else if (params.signedMessages) {
      proto::ConcurrencyControl cc(phase1Reply.cc());
      //Latency_Start(&signLat);
      SignMessage(&cc, keyManager->GetPrivateKey(id), id,
          phase1Reply.mutable_signed_cc());
      Debug("PHASE1FB[%s] Adding FB Phase1Reply with signature %s from priv key %d.",
          BytesToHex(txnDigest, 16).c_str(),
          BytesToHex(phase1Reply.signed_cc().signature(), 100).c_str(), id);
      //Latency_End(&signLat);
    }
  }
}

void Server::SetP2(uint64_t reqId, std::string txnDigest, proto::CommitDecision &decision){
  phase2Reply.Clear();
  phase2Reply.set_req_id(reqId);

  phase2Reply.mutable_p2_decision()->set_decision(decision);
  //TODO: ADD VIEW. Is this the right notation?
  if(decision_views.find(txnDigest) == decision_views.end()) decision_views[txnDigest] = 0;
  phase2Reply.mutable_p2_decision()->set_view(decision_views[txnDigest]);

  if (params.validateProofs) {
    *phase2Reply.mutable_p2_decision()->mutable_txn_digest() = txnDigest;
    if (params.signedMessages) {
      proto::Phase2Decision p2Decision(phase2Reply.p2_decision());
      //Latency_Start(&signLat);
      SignMessage(&p2Decision, keyManager->GetPrivateKey(id), id,
          phase2Reply.mutable_signed_p2_decision());
      //Latency_End(&signLat);
    }
  }
}


void Server::SendPhase1FBReply(uint64_t reqId,
    proto::Phase1Reply &p1r, proto::Phase2Reply &p2r, proto::Writeback &wb,
    const TransportAddress &remote, std::string txnDigest, uint32_t response_case ) {

    //TODO? update fb response set to reply quickly for repetitions?

    phase1FBReply.Clear();
    phase1FBReply.set_req_id(reqId);
    phase1FBReply.set_txn_digest(txnDigest);

    switch(response_case){
      //commit or abort done --> writeback
      case 1:
          *phase1FBReply.mutable_wb() = wb;
          //phase1FBReply.mutable_wb(wb);
          break;
      //p2 and p1 decision.
      case 2:
          *phase1FBReply.mutable_p2r() = p2r;
          *phase1FBReply.mutable_p1r() = p1r;
          // phase1FBReply.mutable_p2r(p2r);
          // phase1FBReply.mustable_p1r(p1r);
          break;
      //p2 only
      case 3:
          *phase1FBReply.mutable_p2r() = p2r;
          //phase1FBReply.mutable_p2r(p2r);
          break;
      //p1 only
      case 4:
          *phase1FBReply.mutable_p1r() = p1r;
          //phase1FBReply.mutable_p1r(p1r);
          break;
    }
    //proto::CurrentView curr_view;
    //curr_view.mutable_txn_digest(txnDigest);
    //curr_view.set_current_view(current_views[txnDigest]);
    //*phase1FBReply.mutable_current_view() = *curr_view;
    //phase1FBReply.mutable_current_view(curr_view));
    proto::AttachedView attachedView;
    attachedView.mutable_current_view()->set_current_view(current_views[txnDigest]);
    attachedView.mutable_current_view()->set_replica_id(id);

//SIGN IT:  //TODO: Want to add this to p2 also, in case fallback is faulty - for simplicity assume only correct fallbacks for now.
      *attachedView.mutable_current_view()->mutable_txn_digest() = txnDigest; //redundant line if I always include txn digest

      if (params.signedMessages) {
        proto::CurrentView cView(attachedView.current_view());
        //Latency_Start(&signLat);
        SignMessage(&cView, keyManager->GetPrivateKey(id), id, attachedView.mutable_signed_current_view());
        //Latency_End(&signLat);
      }
    *phase1FBReply.mutable_attached_view() = attachedView;

    transport->SendMessage(this, remote, phase1FBReply);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//TODO: refactor this into the function below...
void Server::HandlePhase2FB(const TransportAddress &remote,
    proto::Phase2FB &msg) {
  //std::string txnDigest = TransactionDigest(msg.txn(), params.hashDigest);
  std::string txnDigest = msg.txn_digest();
  //Debug("PHASE2FB[%lu:%lu][%s] with ts %lu.", msg.txn().client_id(),
  //    msg.txn().client_seq_num(), BytesToHex(txnDigest, 16).c_str(),
  //    msg.txn().timestamp().timestamp());

      //currently for simplicity just forward writeback message that we received and stored.
    if(writebackMessages.find(txnDigest) != writebackMessages.end()){
      proto::Writeback wb = writebackMessages[txnDigest];
      SendPhase1FBReply(msg.req_id(), phase1Reply, phase2Reply, wb,  remote, txnDigest, 1); //TODO: Create a seperate message explicitly for writeback: This suffices to finish all fallback procedures
    }

  // HandePhase2 just returns an existing decision.
else if(p2Decisions.find(msg.txn_digest()) != p2Decisions.end()){
      proto::CommitDecision decision =p2Decisions[txnDigest];

      SetP2(msg.req_id(), txnDigest, decision);
      phase2FBReply.Clear();
      phase2FBReply.set_txn_digest(txnDigest);
      *phase2FBReply.mutable_p2r() = phase2Reply;
      proto::AttachedView attachedView;
      attachedView.mutable_current_view()->set_current_view(current_views[txnDigest]);
      attachedView.mutable_current_view()->set_replica_id(id);

      //SIGN IT: .
        *attachedView.mutable_current_view()->mutable_txn_digest() = txnDigest; //redundant line if I always include txn digest

        if (params.signedMessages) {
          proto::CurrentView cView(attachedView.current_view());
          //Latency_Start(&signLat);
          SignMessage(&cView, keyManager->GetPrivateKey(id), id, attachedView.mutable_signed_current_view());
          //Latency_End(&signLat);
        }
      *phase2FBReply.mutable_attached_view() = attachedView;


      transport->SendMessage(this, remote, phase2FBReply);
      Debug("PHASE2FB[%s] Sent Phase2Reply.", BytesToHex(txnDigest, 16).c_str());
}
//just do normal handle p2 otherwise after timeout
else{

      //The timer should start running AFTER the Mvtso check returns.
      // I could make the timeout window 0 if I dont expect byz clients. An honest client will likely only ever start this on conflict.
      //std::chrono::high_resolution_clock::time_point current_time = high_resolution_clock::now();

      //VerifyP2FB(remote, txnDigest, msg)
      transport->Timer((CLIENTTIMEOUT), [this, &remote, &txnDigest, &msg](){VerifyP2FB(remote, txnDigest, msg);});

//TODO: for time being dont do this smarter scheduling.
// if(false){
//       struct timeval tv;
//       gettimeofday(&tv, NULL);
//       uint64_t current_time = (tv.tv_sec*1000000+tv.tv_usec)/1000;  //in miliseconds
//
//
//       //std::time_t current_time;
//       //std::chrono::high_resolution_clock::time_point
//       uint64_t elapsed;
//       if(client_starttime.find(txnDigest) != client_starttime.end())
//           elapsed = current_time - client_starttime[txnDigest];
//       else{
//         //PANIC, have never seen the tx that is mentioned. Start timer ourselves.
//         client_starttime[txnDigest] = current_time;
//         transport->Timer((CLIENTTIMEOUT), [this, &remote, &txnDigest, &msg](){VerifyP2FB(remote, txnDigest, msg);});
//         return;
//
//       }
//
// 	    //current_time = time(NULL);
//       //std::time_t elapsed = current_time - FBclient_timeouts[txnDigest];
//       //TODO: Replay this toy time logic with proper MS timer.
//       if (elapsed >= CLIENTTIMEOUT){
//         VerifyP2FB(remote, txnDigest, msg);
//       }
//       else{
//         //schedule for once original client has timed out.
//         transport->Timer((CLIENTTIMEOUT-elapsed), [this, &remote, &txnDigest, &msg](){VerifyP2FB(remote, txnDigest, msg);});
//         return;
//       }
//  }
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



void Server::VerifyP2FB(const TransportAddress &remote, std::string &txnDigest, proto::Phase2FB &p2fb){
  uint8_t groupIndex = txnDigest[0];
  int64_t logGroup;
  // find txn. that maps to txnDigest. if not stored locally, and not part of the msg, reject msg.
  //TODO: refactor this into common function
   ////if(params.mainThreadDispatching) ongoingMutex.lock();
  ongoingMap::const_accessor b;
  auto ongoingItr = ongoing.find(b, txnDigest);
  if(ongoingItr){
  //if(ongoing.find(txnDigest) != ongoing.end()){
    //proto::Transaction txn = *ongoing[txnDigest];
    proto::Transaction txn = *(b->second);
    b.release();
    groupIndex = groupIndex % txn.involved_groups_size();
    UW_ASSERT(groupIndex < txn.involved_groups_size());
    logGroup = txn.involved_groups(groupIndex);
  }
  else{
    b.release();
    if(p2fb.has_txn()){
        proto::Transaction txn = p2fb.txn();
        groupIndex = groupIndex % txn.involved_groups_size();
        UW_ASSERT(groupIndex < txn.involved_groups_size());
        logGroup = txn.involved_groups(groupIndex);
      }
      else{
         ////if(params.mainThreadDispatching) ongoingMutex.unlock();
        return; // TXN unseen..
      }
  }
   ////if(params.mainThreadDispatching) ongoingMutex.unlock();
//CHANGE CODE TO CLEARLY ACCOUNT FOR THE NEW CASES:
//P2FB either contains GroupedSignatures directly, OR it just contains P2Replies.

//TODO: Case A
if(p2fb.has_p2_replies()){
  proto::P2Replies p2Reps = p2fb.p2_replies();
  uint32_t counter = config.f + 1;
  for(auto & p2_reply : p2Reps.p2replies()){
      proto::Phase2Decision p2dec;
      if (params.signedMessages) {
        if(!p2_reply.has_signed_p2_decision()){ return;}
        proto::SignedMessage sig_msg = p2_reply.signed_p2_decision();
        if(!IsReplicaInGroup(sig_msg.process_id(), logGroup, &config)){ return;}
        p2dec.ParseFromString(sig_msg.data());
        if(p2dec.decision() == p2fb.decision() && p2dec.txn_digest() == p2fb.txn_digest()){
          if(crypto::Verify(keyManager->GetPublicKey(sig_msg.process_id()),
                &sig_msg.data()[0], sig_msg.data().length(), &sig_msg.signature()[0])){ counter--;} else{return;}
        }

      }
      //no sig case:
      else{
        if(p2_reply.has_p2_decision()){
          if(p2_reply.p2_decision().decision() == p2fb.decision() && p2_reply.p2_decision().txn_digest() == p2fb.txn_digest()) counter--;
        }
      }
      if(counter == 0){
        p2Decisions[txnDigest] = p2fb.decision();
        decision_views[txnDigest] = 0;
        break;
      }
  }
}
//TODO: Case B
else if(p2fb.has_grouped_sigs()){
    proto::Transaction txn;
     ////if(params.mainThreadDispatching) ongoingMutex.lock();
    ongoingMap::const_accessor b;
    bool ongoingItr = ongoing.find(b, txnDigest);
    b.release();
    if(ongoingItr){
    //if(ongoing.find(txnDigest) != ongoing.end()){
      //proto::Transaction txn = *ongoing[txnDigest];
      proto::Transaction txn = *(b->second);
      b.release();
    }
    else if(p2fb.has_txn()){
        b.release();
          proto::Transaction txn = p2fb.txn();
    }
    else{
      b.release();
       ////if(params.mainThreadDispatching) ongoingMutex.unlock();
      return; //TXN unseen.
    }
     ////if(params.mainThreadDispatching) ongoingMutex.unlock();

    proto::GroupedSignatures grpSigs = p2fb.grouped_sigs();
    int64_t myProcessId;
    proto::ConcurrencyControl::Result myResult;
    LookupP1Decision(txnDigest, myProcessId, myResult);

    if(!ValidateP1Replies(p2fb.decision(), false, &txn, &txnDigest, grpSigs, keyManager, &config, myProcessId, myResult, verifier)){
      return; //INVALID SIGS
    }
    p2Decisions[txnDigest] = p2fb.decision();
    decision_views[txnDigest] = 0;
}

if(p2Decisions.find(txnDigest) == p2Decisions.end()) return; //Still no decision. Failure.
// form p2r and send it
SetP2(p2fb.req_id(), txnDigest, p2Decisions[txnDigest]);
phase2FBReply.Clear();
phase2FBReply.set_txn_digest(txnDigest);
*phase2FBReply.mutable_p2r() = phase2Reply;
proto::AttachedView attachedView;
attachedView.mutable_current_view()->set_current_view(current_views[txnDigest]);
attachedView.mutable_current_view()->set_replica_id(id);

//SIGN IT:  //TODO: Want to add this to p2 also, in case fallback is faulty - for simplicity assume only correct fallbacks for now.
  *attachedView.mutable_current_view()->mutable_txn_digest() = txnDigest; //redundant line if I always include txn digest

  if (params.signedMessages) {
    proto::CurrentView cView(attachedView.current_view());
    //Latency_Start(&signLat);
    SignMessage(&cView, keyManager->GetPrivateKey(id), id, attachedView.mutable_signed_current_view());
    //Latency_End(&signLat);
  }
*phase2FBReply.mutable_attached_view() = attachedView;


transport->SendMessage(this, remote, phase2FBReply);
Debug("PHASE2FB[%s] Sent Phase2Reply.", BytesToHex(txnDigest, 16).c_str());

}
///OLD CODE:

// //  if (params.validateProofs) {
//   //check if there are p2 decisions.
//    proto::GroupedP1FBreplies groupedReplies = p2fb.grp_p1_fb();
//    //proto::P1FBreplies loggedReplies = groupedReplies.grouped_fbreplies()[logGroup];
//     //TRY THIS:  (*groupedReplies.mutable_grouped_fbreplies())[logGroup]
//
// //construct decision object. serialize it. compare.
//    uint32_t counter = config.f + 1; //or config.QuorumSize()
//
//    for(auto & loggedReplies : groupedReplies.grouped_fbreplies()){
//      if(loggedReplies.first != logGroup) continue;
//
//      for(const auto &p1fbr : loggedReplies.second.fbreplies()){
//           if(p1fbr.has_p2r()){
//             proto::Phase2Reply p2r = p1fbr.p2r();
//
//               if (params.signedMessages) {
//                 if(p2r.has_signed_p2_decision()){
//                   proto::SignedMessage sig_msg = p2r.signed_p2_decision();
//                   proto::Phase2Decision p2Dec;
//                   p2Dec.ParseFromString(sig_msg.data());
//                   if(p2Dec.decision() == p2fb.decision() && p2Dec.txn_digest() == p2fb.txn_digest()){
//                     if(crypto::Verify(keyManager->GetPublicKey(sig_msg.process_id()), sig_msg.data(), sig_msg.signature())){ counter--;} else{return;}
//                   }
//                 }
//               }
//               else{
//                 if(p2r.has_p2_decision()){
//                   if(p2r.p2_decision().decision() == p2fb.decision() && p2r.p2_decision().txn_digest() == p2fb.txn_digest()) counter--;
//                 }
//               }
//           }
//           if(counter == 0){
//             p2Decisions[txnDigest] = p2fb.decision();
//             decision_views[txnDigest] = 0;
//             break;
//           }
//         }
//     }
//     //still no decision --> check all p1s.
//     proto::GroupedSignatures grpSigs;
//     //std::unordered_map<uint64_t, proto::Signatures> grp_sigs;
//     if(p2Decisions.find(txnDigest) == p2Decisions.end()){
//           //TODO: extract all p1s as proof and call HandleP2
//           for(auto & group_replies: groupedReplies.grouped_fbreplies()){
//           //for(auto & [ key, group_replies ]: p2fb.grp_p1_f()->grouped_fbreplies()){
//             proto::Signatures sig_collection;
//             for (auto &p1fbr: group_replies.second.fbreplies()){
//             //for(auto p1fbr: group_replies.second()){
//               proto::Signature *new_sig = sig_collection.add_sigs();
//               //new_sig.mutable_signature(p1fbr.p1r()->signed_cc()->signature()); //Is this correct.
//               *new_sig->mutable_signature() = p1fbr.p1r().signed_cc().signature();
//
//             }
//             //grp_sigs[key] = sig_collection;
//             //grp_sigs[group_replies.first] = sig_collection;
//             (*grpSigs.mutable_grouped_sigs())[group_replies.first] = sig_collection;
//           }
//         //grpSigs.mutable_grouped_sigs(grp_sigs);
//       //  *grpSigs.mutable_grouped_sigs() = grp_sigs;
//
//         proto::Transaction txn;
//         if(ongoing.find(txnDigest) != ongoing.end()){
//           proto::Transaction txn = *ongoing[txnDigest];
//         }
//         else if(p2fb.has_txn()){
//               proto::Transaction txn = p2fb.txn();
//         }
//         else{
//           return; //TXN unseen.
//         }
//
//         int64_t myProcessId;
//         proto::ConcurrencyControl::Result myResult;
//         LookupP1Decision(txnDigest, myProcessId, myResult);
//
//         if(!ValidateP1Replies(p2fb.decision(), false, &txn, &txnDigest, grpSigs, keyManager, &config, myProcessId, myResult)){
//           return; //INVALID SIGS
//         }
//         p2Decisions[txnDigest] = p2fb.decision();
//         decision_views[txnDigest] = 0;
//     }


//TODO: Views are now signed messages. Split verify P2fb function.
bool Server::VerifyViews(proto::InvokeFB &msg, uint32_t lG){

  auto txnDigest = msg.txn_digest();

  //Assuming Invoke Message contains SignedMessages for view instead of Signatures.
      proto::SignedMessages signed_messages = msg.view_signed();
      if(msg.catchup()){
        uint64_t counter = config.f +1;
        for(const auto &signed_m : signed_messages.sig_msgs()){

          proto::CurrentView view_s;
          view_s.ParseFromString(signed_m.data());

          if(IsReplicaInGroup(signed_m.process_id(), lG, &config)){
              if(view_s.current_view() < msg.proposed_view()){ return false;}
              if(view_s.txn_digest() != txnDigest) { return false;}
              if(crypto::Verify(keyManager->GetPublicKey(signed_m.process_id()),
                    &signed_m.data()[0], signed_m.data().length(), &signed_m.signature()[0])) { counter--;} else{return false;}
          }
          if(counter == 0) return true;
        }
      }
      else{
        uint64_t counter = 3* config.f +1;
        for(const auto &signed_m : signed_messages.sig_msgs()){

          proto::CurrentView view_s;
          view_s.ParseFromString(signed_m.data());

          if(IsReplicaInGroup(signed_m.process_id(), lG, &config)){
              if(view_s.current_view() < msg.proposed_view()-1){ return false;}
              if(view_s.txn_digest() != txnDigest) { return false;}
              if(crypto::Verify(keyManager->GetPublicKey(signed_m.process_id()),
                    &signed_m.data()[0], signed_m.data().length(),
                    &signed_m.signature()[0])) { counter--;} else{return false;}
          }
          if(counter == 0) return true;
        }

      }
      return false;


  // //USE THIS CODE IF ASSUMING VIEWS ARE JUST SIGNATURES - NEED TO DISTINGUISH THOUGH IN CASE VIEWS WHERE >= proposed view, not just =
  // std::string viewMsg;
  // proto::CurrentView curr_view;
  //curr_view.mutable_txn_digest(msg.txn_digest());

  //*curr_view.mutable_txn_digest() = txnDigest;

  //   proto::Signatures sigs = msg.view_sigs();
  //   if(msg.catchup()){
  //     curr_view.set_current_view(msg.proposed_view());
  //     curr_view.SerializeToString(&viewMsg);
  //     uint64_t counter = config.f +1;
  //     for(const auto &sig : sigs.sigs()){
  //       //TODO: check that this id was from the loggin shard. sig.process_id() in lG
  //       if(IsReplicaInGroup(sig.process_id(), lG, &config)){
  //           if(crypto::Verify(keyManager->GetPublicKey(sig.process_id()), viewMsg, sig.signature())) { counter--;} else{return false;}
  //       }
  //       if(counter == 0) return true;
  //     }
  //   }
  //   else{
  //     curr_view.set_current_view(msg.proposed_view()-1);
  //     curr_view.SerializeToString(&viewMsg);
  //     uint64_t counter = 3* config.f +1;
  //     for(const auto &sig : sigs.sigs()){
  //       if(IsReplicaInGroup(sig.process_id(), lG, &config)){
  //           if(crypto::Verify(keyManager->GetPublicKey(sig.process_id()), viewMsg, sig.signature() )) { counter--;} else{return false;}
  //       }
  //       if(counter == 0) return true;
  //     }
  //
  //   }
  //   return false;

}


//TODO remove remote argument, it is useless here. Instead add and keep track of INTERESTED CLIENTS REMOTE MAP
void Server::HandleInvokeFB(const TransportAddress &remote, proto::InvokeFB &msg) {
//Expect the invokeFB message to contain a P2 message if not seen a decision ourselves.
//If that is not the case: CALL HAndle Phase2B. This in turn should check if there are f+1 matching p2, and otherwise call HandlePhase2 by passing the args.

proto::Phase2FB p2fb;
// CHECK if part of logging shard. (this needs to be done at all p2s, reject if its not ourselves)
auto txnDigest = msg.txn_digest();

if(msg.proposed_view() <= current_views[txnDigest]) return; //Obsolete Invoke Message. //TODO: return new view.

  //TODO  Schedule request for when current leader timeout is complete --> check exp timeouts; then set new one.
//   struct timeval tv;
//   gettimeofday(&tv, NULL);
//   uint64_t current_time = (tv.tv_sec*1000000+tv.tv_usec)/1000;  //in miliseconds
//
//   uint64_t elapsed;
//   if(client_starttime.find(txnDigest) != client_starttime.end())
//       elapsed = current_time - client_starttime[txnDigest];
//   else{
//     //PANIC, have never seen the tx that is mentioned. Start timer ourselves.
//     client_starttime[txnDigest] = current_time;
//     transport->Timer((CLIENTTIMEOUT), [this, &remote, &msg](){HandleInvokeFB(remote, msg);});
//     return;
//   }
//   if (elapsed < CLIENTTIMEOUT ){
//     transport->Timer((CLIENTTIMEOUT-elapsed), [this, &remote, &msg](){HandleInvokeFB(remote, msg);});
//     return;
//   }
// //check for current FB reign
//   uint64_t FB_elapsed;
//   if(exp_timeouts.find(txnDigest) != exp_timeouts.end()){
//       FB_elapsed = current_time - FBtimeouts_start[txnDigest];
//       if(FB_elapsed < exp_timeouts[txnDigest]){
//           transport->Timer((exp_timeouts[txnDigest]-FB_elapsed), [this, &remote, &msg](){HandleInvokeFB(remote, msg);});
//           return;
//       }
//
//   }
  //otherwise pass and invoke for the first time!


      uint8_t groupIndex = txnDigest[0];
      int64_t logGroup;
      // find txn. that maps to txnDigest. if not stored locally, and not part of the msg, reject msg.
      //TODO: refactor this into common function

       ////if(params.mainThreadDispatching) ongoingMutex.lock();

      ongoingMap::const_accessor b;
      bool ongoingItr = ongoing.find(b, txnDigest);
      if(ongoingItr){
      //if(ongoing.find(txnDigest) != ongoing.end()){
      //  proto::Transaction txn = *ongoing[txnDigest];
      proto::Transaction txn = *b->second;
        groupIndex = groupIndex % txn.involved_groups_size();
        UW_ASSERT(groupIndex < txn.involved_groups_size());
        logGroup = txn.involved_groups(groupIndex);
      }
      else if(msg.has_p2fb()){
        p2fb = msg.p2fb();
        if(p2fb.has_txn()){
            proto::Transaction txn = p2fb.txn();
            groupIndex = groupIndex % txn.involved_groups_size();
            UW_ASSERT(groupIndex < txn.involved_groups_size());
            logGroup = txn.involved_groups(groupIndex);
          }
        else{
           ////if(params.mainThreadDispatching) ongoingMutex.unlock();
          return; //REPLICA HAS NEVER SEEN THIS TXN
        }
      }
      else{
         ////if(params.mainThreadDispatching) ongoingMutex.unlock();
        return; //REPLICA HAS NEVER SEEN THIS TXN
      }
      b.release();
       ////if(params.mainThreadDispatching) ongoingMutex.unlock();

      if(groupIdx != logGroup){
          return;  //This replica is not part of the shard responsible for Fallback.
      }



//process decision if one does not have any yet.
//This is safe even if current_view > 0 because this replica could not have taken part in any elections yet (can only elect once you have decision), nor has yet received a dec from a larger view which it would adopt.
      if(p2Decisions.find(txnDigest) == p2Decisions.end()){
        p2fb = msg.p2fb();
        HandlePhase2FB(remote, p2fb);
        //either no need for fallback, or still no decision learned so one cannot contribute to election.
        if(writebackMessages.find(txnDigest) != writebackMessages.end() || p2Decisions.find(txnDigest) == p2Decisions.end()){
          return;
        }
      }

    if(msg.proposed_view() <= current_views[txnDigest]) return; //already voted or passed this view. //TODO: reply with signed current current_view..



//verify views
    if(!VerifyViews(msg, logGroup)) return; //invalid view signatures.
    //TODO: Adopt view if it is bigger than our current.
    if(current_views[txnDigest] >= msg.proposed_view()) return; //
    current_views[txnDigest] = msg.proposed_view();
    size_t replicaID = (msg.proposed_view() + txnDigest[0]) % config.n;

    //TODO 3) Form and send ElectFB message to all replicas within logging shard.
    proto::ElectMessage electMessage;
    electMessage.set_req_id(msg.req_id());
    electMessage.set_txn_digest(txnDigest);
    electMessage.set_decision(p2Decisions[txnDigest]);
    electMessage.set_view(msg.proposed_view());

    proto::ElectFB electFB;
    *electFB.mutable_elect_fb() = electMessage;

    if (params.signedMessages) {
      SignMessage(&electMessage, keyManager->GetPrivateKey(id), id, electFB.mutable_signed_elect_fb());
      transport->SendMessageToReplica(this, logGroup, replicaID, electFB);
    }
    else{
      transport->SendMessageToReplica(this, logGroup, replicaID, electFB);
    }

    //Set timeouts new.
    // gettimeofday(&tv, NULL);
    // current_time = (tv.tv_sec*1000000+tv.tv_usec)/1000;
    // if(exp_timeouts.find(txnDigest) == exp_timeouts.end()){
    //    exp_timeouts[txnDigest] = CLIENTTIMEOUT; //start first timeout. //TODO: Make this a config parameter.
    //    FBtimeouts_start[txnDigest] = current_time;
    // }
    // else{
    //    exp_timeouts[txnDigest] = exp_timeouts[txnDigest] * 2; //TODO: increase timeouts exponentially. SET INCREASE RATIO IN CONFIG
    //    FBtimeouts_start[txnDigest] = current_time;
    // }
    //(TODO 4) Send MoveView message for new view to all other replicas)
}

//TODO remove remote argument
void Server::HandleElectFB(const TransportAddress &remote, proto::ElectFB &msg){

  //assume all is signed for now. TODO: make that general so no exceptions.
  if (!params.signedMessages) {Debug("ERROR HANDLE ELECT FB: NON SIGNED VERSION NOT IMPLEMENTED");}
  if(!msg.has_signed_elect_fb()) return;

  proto::SignedMessage signed_msg = msg.signed_elect_fb();
  proto::ElectMessage electMessage;
  electMessage.ParseFromString(signed_msg.data());
  std::string Msg;
  electMessage.SerializeToString(&Msg);
  std::string txnDigest = electMessage.txn_digest();

  if(ElectQuorum.find(txnDigest) == ElectQuorum.end()){
    //ElectQuorum[txnDigest] = std::unordered_set<proto::SignedMessage*>();
    ElectQuorum_meta[txnDigest] = std::make_pair(0, 0);
  }

  if(uint64_t(idx) != (electMessage.view() + txnDigest[0]) % config.n) return;
  if(IsReplicaInGroup(signed_msg.process_id(), groupIdx, &config)){
    if(ElectQuorum_meta[txnDigest].first > electMessage.view()) return;

    if(!crypto::Verify(keyManager->GetPublicKey(signed_msg.process_id()),
          &Msg[0], Msg.length(), &signed_msg.signature()[0])) return;
    // check whether all views in the elect messages match (the replica)
    if(ElectQuorum_meta[txnDigest].first == electMessage.view() && ElectQuorum[txnDigest].find(&signed_msg) == ElectQuorum[txnDigest].end()){
        // update state, keep counter of received ElectFbs  --> last one executes the function
      ElectQuorum[txnDigest].insert(&signed_msg); //no clone?
      if(electMessage.decision() == proto::COMMIT) ElectQuorum_meta[txnDigest].second++;
    }
    else if(ElectQuorum_meta[txnDigest].first < electMessage.view()){
      ElectQuorum_meta[txnDigest].first = electMessage.view();
      ElectQuorum[txnDigest].clear(); //TODO:free all.
      ElectQuorum[txnDigest].insert(&signed_msg); //no clone?
    }
  }

//form new decision, and send DecisionFB message to all replicas
  if(ElectQuorum[txnDigest].size() == uint64_t(config.n - config.f)){
    proto::CommitDecision decision;
    if(ElectQuorum_meta[txnDigest].second > uint64_t(2* config.f +1)){
      decision = proto::COMMIT;
    }
    else{
      decision = proto::ABORT;
    }
    proto::DecisionFB decisionFB;
    decisionFB.set_req_id(electMessage.req_id());
    decisionFB.set_txn_digest(txnDigest);
    decisionFB.set_dec(decision);
    decisionFB.set_view(electMessage.view());
    for (const proto::SignedMessage *iter : ElectQuorum[txnDigest] ) {
      //response->add_elect_sigs(iter);
      proto::SignedMessage* sm = decisionFB.add_elect_sigs();
      *sm = *iter;
    }
    transport->SendMessageToGroup(this, groupIdx, decisionFB);

  }
}

void Server::HandleDecisionFB(const TransportAddress &remote,
     proto::DecisionFB &msg){

    std::string txnDigest = msg.txn_digest();
    //outdated request
    if(current_views[txnDigest] > msg.view()) return;


    std::string Msg;
  //  proto::ElectFB electfb;

    proto::ElectMessage elect_msg;

//TODO: verify signatures, Ignore if from view < current
    uint64_t counter = 2* config.f +1;
    for(auto & iter :msg.elect_sigs()){  //iter of type SignedMessage
    //  electfb.ParseFromString(iter.data());
    //  electfb.SerializeToString(&Msg);
      elect_msg.ParseFromString(iter.data());
      //TODO: need to add that these signatures come from different replicas..
      //TODO: check that there are 4f+1 elect messages in total? is this necessary?
      if(crypto::Verify(keyManager->GetPublicKey(iter.process_id()),
            &iter.data()[0], iter.data().length(), &iter.signature()[0])){
        //check that replicas were electing this FB and voting for this decision
        //if(electfb.elect_fb().decision() == msg.dec() && electfb.elect_fb().view() == msg.view()){
        if(elect_msg.decision() == msg.dec() && elect_msg.view() == msg.view()){
          counter--;
        }
      }
      if(counter==0) break;
    }
    if(counter !=0) return; //sigs dont match decision

    if(decision_views[txnDigest] < msg.view()){
      decision_views[txnDigest] = msg.view();
      p2Decisions[txnDigest] = msg.dec();
    }
    if(current_views[txnDigest] < msg.view()){
      current_views[txnDigest] = msg.view();
    }

    SetP2(msg.req_id(), txnDigest, p2Decisions[txnDigest]);  //TODO: problematic that client cannot distinguish? Wrap it inside a P2FBreply?
    phase2FBReply.Clear();
    phase2FBReply.set_txn_digest(txnDigest);
    *phase2FBReply.mutable_p2r() = phase2Reply;
    proto::AttachedView attachedView;
    attachedView.mutable_current_view()->set_current_view(current_views[txnDigest]);
    attachedView.mutable_current_view()->set_replica_id(id);

//SIGN IT:  //TODO: Want to add this to p2 also, in case fallback is faulty - for simplicity assume only correct fallbacks for now.
      *attachedView.mutable_current_view()->mutable_txn_digest() = txnDigest; //redundant line if I always include txn digest

      if (params.signedMessages) {
        proto::CurrentView cView(attachedView.current_view());
        //Latency_Start(&signLat);
        SignMessage(&cView, keyManager->GetPrivateKey(id), id, attachedView.mutable_signed_current_view());
        //Latency_End(&signLat);
      }
    *phase2FBReply.mutable_attached_view() = attachedView;

    interestedClientsMap::const_accessor i;
    bool interestedClientsItr = interestedClients.find(i, txnDigest);

    for(const TransportAddress * target :   i->second ){
      transport->SendMessage(this, *target, phase2FBReply);
    }
    i.release();
  //TODO: 2) Send Phase2Reply message to all interested clients (check list interestedClients for all remote addresses)
  //TODO: 3) send current views to client (in case there was no consensus, so the client can start a new round.)
}







///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//RELAY DEPENDENCY IN ORDER FOR CLIENT TO START FALLBACK
void Server::RelayP1(const TransportAddress &remote, std::string txnDigest, uint64_t conflict_id){

  Debug("RelayP1[%s] timed out.", BytesToHex(txnDigest, 256).c_str());
  proto::Transaction *tx;

  ongoingMap::const_accessor b;
  ////auto ongoingMutexScope = params.mainThreadDispatching ? std::shared_lock<std::shared_mutex>(ongoingMutex) : std::shared_lock<std::shared_mutex>();
  //if (ongoing.find(txnDigest) == ongoing.end()) return;
  bool ongoingItr = ongoing.find(b, txnDigest);
  if(!ongoingItr) return;

  //tx = ongoing[txnDigest];
  tx = b->second;
  b.release();
  proto::Phase1 p1;
  p1.set_req_id(0); //doesnt matter, its not used for fallback requests really.
  *p1.mutable_txn() = *tx;
  proto::RelayP1 relayP1;
  relayP1.set_conflict_id(conflict_id);
  *relayP1.mutable_p1() = p1;

  Debug("Sending RelayP1[%s].", BytesToHex(txnDigest, 256).c_str());

  transport->SendMessage(this, remote, relayP1);
}



} // namespace indicusstore
