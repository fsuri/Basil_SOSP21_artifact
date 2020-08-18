#include "store/indicusstore/localbatchverifier.h"

#include "lib/crypto.h"
#include "lib/batched_sigs.h"
#include "store/indicusstore/common.h"
#include "lib/message.h"

namespace indicusstore {

LocalBatchVerifier::LocalBatchVerifier(uint64_t merkleBranchFactor, Stats &stats, Transport* transport) :
  merkleBranchFactor(merkleBranchFactor), stats(stats), transport(transport), batchTimeoutMicro(ULONG_MAX) {
  _Latency_Init(&hashLat, "hash");
  _Latency_Init(&cryptoLat, "crypto");
}

LocalBatchVerifier::LocalBatchVerifier(uint64_t merkleBranchFactor, Stats &stats, Transport* transport,
   uint64_t batchTimeoutMicro, bool adjustBatchSize, uint64_t batch_size) :
  merkleBranchFactor(merkleBranchFactor), stats(stats), transport(transport),
  batchTimerRunning(false), batch_size(batch_size), messagesBatchedInterval(0UL), batchTimeoutMicro(batchTimeoutMicro) {
    _Latency_Init(&hashLat, "hash");
    _Latency_Init(&cryptoLat, "crypto");
    if (adjustBatchSize) {
      transport->TimerMicro(batchTimeoutMicro, std::bind(
          &LocalBatchVerifier::AdjustBatchSize, this));
    }
}


LocalBatchVerifier::~LocalBatchVerifier() {
  Latency_Dump(&hashLat);
  Latency_Dump(&cryptoLat);
}

bool LocalBatchVerifier::Verify(crypto::PubKey *publicKey, const std::string &message,
    const std::string &signature) {    //TODO  ADD CALLBACK as argument, needs to be passed to batcher. ()
  std::string hashStr;
  std::string rootSig;
  Latency_Start(&hashLat);
  if (!BatchedSigs::computeBatchedSignatureHash(&signature, &message, publicKey,
      hashStr, rootSig, merkleBranchFactor)) {
    return false;
  }
  Latency_End(&hashLat);
  auto itr = cache.find(rootSig);
  if (itr == cache.end()) {
    stats.Increment("verify_cache_miss");
    Latency_Start(&cryptoLat);
    //TODO: here: call AddToBatch  . This function needs to include the callback of the return
    //TODO: add automatic dispatch/ VerifyBatch when batch is full. Pass Callback function that manages all the callbacks.
    if (crypto::Verify(publicKey, &hashStr[0], hashStr.length(), &rootSig[0])) {
      Latency_End(&cryptoLat);
      cache[rootSig] = hashStr;
      return true;
    } else {
      Latency_End(&cryptoLat);
      Debug("Verification with public key failed.");
      return false;
    }
  } else {
    if (hashStr == itr->second) {
      stats.Increment("verify_cache_hit");
      return true;
    } else {
      Debug("Verification via cached hash %s failed.",
          BytesToHex(itr->second, 100).c_str());
      return false;
    }
  }
}

void* LocalBatchVerifier::asyncComputeBatchVerification(std::vector<crypto::PubKey*> _publicKeys,
  std::vector<const char*> _messages, std::vector<size_t> _messageLens,
  std::vector<const char*> _signatures, int _current_fill){
    UW_ASSERT(_current_fill>0);
    UW_ASSERT(_publicKeys[0]->t == crypto::KeyType::DONNA);
    int* valid = new int[_current_fill];
    bool all_valid = crypto::BatchVerify(crypto::KeyType::DONNA, _publicKeys.data(), _messages.data(), _messageLens.data(), _signatures.data(), _current_fill, valid);

    return (void*) valid;
}

void* LocalBatchVerifier::asyncComputeBatchVerificationS(std::vector<crypto::PubKey*> _publicKeys,
  std::vector<std::string*> _messagesS, std::vector<size_t> _messageLens,
  std::vector<std::string*> _signaturesS, int _current_fill){
    UW_ASSERT(_current_fill>0);
    UW_ASSERT(_publicKeys[0]->t == crypto::KeyType::DONNA);
    int* valid = new int[_current_fill];

    bool all_valid = crypto::BatchVerifyS(crypto::KeyType::DONNA, _publicKeys.data(), _messagesS.data(),
        _messageLens.data(), _signaturesS.data(), _current_fill, valid);
    Debug("All valid: %s", all_valid? "true" : "false");
        // for(int i =0; i<_current_fill; ++i){
        //
        //   delete _messages[i];
        //   delete _signatures[i];
        // }
    return (void*) valid;
}


//hashStr = message, rootSig = signature
void LocalBatchVerifier::asyncBatchVerifyCallback(crypto::PubKey *publicKey, std::string *hashStr,
  std::string *rootSig, verifyCallback vb, bool multithread, bool autocomplete,  void* validate){

  Debug("hashStr pointer: %p", hashStr);
  Debug("rootSig pointer: %p", rootSig);
  Debug("Printing hashStr: %s", BytesToHex(*hashStr, 1028).c_str());
  Debug("Printing rootSig: %s", BytesToHex(*rootSig, 1028).c_str());

  // std::string message(*hashStr);
  // std::string signature(*rootSig);
  // delete hashStr;
  // delete rootSig;
  // delete msg_copy;
  // delete sig_copy;

Debug("Harry you're a wizard? %s", *(bool*)validate ? "yes" : "no" );
//TODO modify accordingly to params.
  if(*(bool*) validate){
    Latency_End(&hashLat);
    auto itr = cache.find(*rootSig);

    if (itr != cache.end()) {
      if (*hashStr == itr->second) {
        stats.Increment("verify_cache_hit");
        bool *res = new bool(true);
        vb((void*)res);
        delete hashStr;
        delete rootSig;
        delete (bool*) validate;
        return;
      } else {
        Debug("Verification via cached hash %s failed.",
            BytesToHex(itr->second, 100).c_str());
        bool *res = new bool(false);
        vb((void*)res);
        delete hashStr;
        delete rootSig;
        delete (bool*) validate;
        return;
      }
    }

    stats.Increment("verify_cache_miss");

    if (batch_size == 1) {
        Debug("Initial batch size = 1, immediately verifying");
        //TODO: add dispatching
        if(multithread){
          //std::function<bool()> func(std::bind(&Verifier::Verify, this, publicKey, message, signature));
          std::string msg(*hashStr);
          std::string sig(*rootSig);
          std::function<bool()> func(std::bind(&Verifier::Verify, this, publicKey, msg, sig));
          std::function<void*()> f(std::bind(pointerWrapperC<bool>, func));
          transport->DispatchTP(f, vb);
        }
        else{
          //std::function<bool()> func(std::bind(&Verifier::Verify, this, publicKey, message, signature));
          //std::function<bool()> func(std::bind(&Verifier::Verify, this, publicKey, *hashStr, *rootSig));
          bool res = new bool(Verify(publicKey, *hashStr, *rootSig));
          //Verify(publicKey, message, signature)
          //void* res = pointerWrapperC<bool>(func);
          vb((void*)res);
        }
        delete hashStr;
        delete rootSig;
      }
    else {
      UW_ASSERT(publicKey->t == crypto::KeyType::DONNA);

      messagesBatchedInterval++;

      publicKeys.push_back(publicKey);
      messagesS.push_back(hashStr);
      signaturesS.push_back(rootSig);
      //messages.push_back(&message[0]);
      //messageLens.push_back(message.length());
      messageLens.push_back((*hashStr).length());
      //signatures.push_back(&signature[0]);
      pendingBatchCallbacks.push_back(vb);

      current_fill++;

      if(autocomplete && current_fill >= batch_size) {
        Debug("Batch is full, verifying");
        if (batchTimerRunning) {
          transport->CancelTimer(batchTimerId);
          batchTimerRunning = false;
        }
        Complete(multithread);
      }
      else if (autocomplete && !batchTimerRunning) {
        batchTimerRunning = true;
        Debug("Starting batch timer");
        batchTimerId = transport->TimerMicro(batchTimeoutMicro, [this, multithread]() {
          Debug("Batch timer expired with %d items, verifying",
              this->current_fill);
          this->batchTimerRunning = false;
          this->Complete(multithread, true);
        });
      }
    }
  }
  delete (bool*) validate;
}
//correct the multithreading function dispatching.

//might want to change message and signature to call by value?
void LocalBatchVerifier::asyncBatchVerify(crypto::PubKey *publicKey, const std::string &message,
    const std::string &signature, verifyCallback vb, bool multithread, bool autocomplete){
      //autocomplete param indicates whether complete should be called as soon as a batch is full
      //Allows us to ignore batch limit and batch all together.


    std::string *hashStr = new std::string;
    std::string *rootSig = new std::string;
    // const std::string *msg_copy = new std::string(message);
    // //*msg_copy = message;
    // const std::string *sig_copy = new std::string(signature);
    // //*sig_copy = signature;



    Debug("hashStr pointer: %p", hashStr);
    Debug("rootSig pointer: %p", rootSig);

//TODO:: Make semantics Multithread compatible... Current problem: main calls Complete but result might not yet be back
    if(multithread){
      std::function<bool()> func(std::bind(BatchedSigs::computeBatchedSignatureHash2<std::string>, signature, message, publicKey,
                hashStr, rootSig, merkleBranchFactor));
      std::function<void*()> f(std::bind(pointerWrapperC<bool>, func));

      std::function<void(void*)> cb(std::bind(&LocalBatchVerifier::asyncBatchVerifyCallback, this, publicKey, hashStr,
         rootSig, vb, multithread, autocomplete, std::placeholders::_1));
      Latency_Start(&hashLat); //inaccurate since the actual execution might be delayed... can pass it to actual function if desired. Currently measures Latency until progress is made, not raw hash time.
      transport->DispatchTP(f, cb);
    }
    else{
      Latency_Start(&hashLat);
       if (BatchedSigs::computeBatchedSignatureHash(&signature, &message, publicKey,
           *hashStr, *rootSig, merkleBranchFactor)){
             bool *validate = new bool(true);
             asyncBatchVerifyCallback(publicKey, hashStr, rootSig, vb, multithread, autocomplete, (void*) validate);

           }

    }
}

void LocalBatchVerifier::Complete(bool multithread, bool force_complete){

  Debug("TRYING TO CALL COMPLETE WITH FILL: %d", current_fill);

  if(force_complete || current_fill >= batch_size) {

    if (batchTimerRunning) {
      transport->CancelTimer(batchTimerId);
      batchTimerRunning = false;
    }

    if(multithread){
      Debug("DISPATCHING BATCH VERIFICATION WITH FILL: %d", current_fill);
      //std::function<void*()> f(std::bind(&LocalBatchVerifier::asyncComputeBatchVerification, this, publicKeys, messages, messageLens, signatures, current_fill));
      //std::function<void(void*)> cb(std::bind(&LocalBatchVerifier::manageCallbacks, this,
      //  messages, signatures, pendingBatchCallbacks, std::placeholders::_1));
      std::function<void*()> f(std::bind(&LocalBatchVerifier::asyncComputeBatchVerificationS, this, publicKeys,
          messagesS, messageLens, signaturesS, current_fill));
      std::function<void(void*)> cb(std::bind(&LocalBatchVerifier::manageCallbacksS, this,
        messagesS, signaturesS, pendingBatchCallbacks, std::placeholders::_1));
      transport->DispatchTP(f, cb);
    }
    else{
      Debug("TRYING TO CALL BATCH VERIFICATION WITH FILL: %d", current_fill);
      //cb(f()); //if one wants to bind always this line suffices
      //if trying to avoid the copying from binding, call with args:
      void* valid = asyncComputeBatchVerificationS(publicKeys, messagesS, messageLens, signaturesS, current_fill);
      Debug("Validation complete");
      manageCallbacksS(messagesS, signaturesS, pendingBatchCallbacks, valid);

      //Below: char* version
      //void* valid = asyncComputeBatchVerification(publicKeys, messages, messageLens, signatures, current_fill);
      //manageCallbacks(messages, signatures, pendingBatchCallbacks, valid);
    }
    publicKeys.clear();
    // for(int i =0; i<current_fill; ++i){
    //   //free((void*)messages[i]);
    //   //free((void*)signatures[i]);
    //   delete messagesS[i];
    //   delete signaturesS[i];
    // }
    messagesS.clear();
    signaturesS.clear();

    //messages.clear();
    messageLens.clear();
    //signatures.clear();
    pendingBatchCallbacks.clear();
    current_fill = 0;
  }

  else if (!batchTimerRunning) {
    batchTimerRunning = true;
    Debug("Starting batch timer");
    batchTimerId = transport->TimerMicro(batchTimeoutMicro, [this, multithread]() {
      Debug("Batch timer expired with %d items, verifying",
          this->current_fill);
      this->batchTimerRunning = false;
      this->Complete(multithread, true);
    });
  }
}


void LocalBatchVerifier::manageCallbacks(std::vector<const char*> _messages, std::vector<const char*> _signatures,
   std::vector<verifyCallback> _pendingBatchCallbacks, void* valid_array){

  int* valid = (int*) valid_array;
  // int valid_size = sizeof(valid) / sizeof(valid[0]);
  //int cb_size = _pendingBatchCallbacks.size();
  // UW_ASSERT(cb_size == valid_size);

  //currently need to call the callbacks for failure results too. If one keeps a global datastructure instead of
  //a dynamic verificationObj then one would "know" if is deleted already or not.
  Debug("Call manageCallbacks for %d items", _pendingBatchCallbacks.size());
  for (int i = 0; i < _pendingBatchCallbacks.size(); ++i){
      bool* res = new bool;
      if(valid[i]){
        std::string hashStr(_signatures[i]);
        std::string rootSig(_messages[i]);
        cache[hashStr] = rootSig;
        bool* res = new bool(true);
        _pendingBatchCallbacks[i]((void*) res);
      }
      else{
        bool* res = new bool(false);
        _pendingBatchCallbacks[i]((void*) res);
      }
  }
  delete [] valid;

  for(int i =0; i< _messages.size(); ++i){
    free((void*)_messages[i]);
    free((void*)_signatures[i]);

  }

}

void LocalBatchVerifier::manageCallbacksS(std::vector<std::string*> _messagesS, std::vector<std::string*> _signaturesS,
   std::vector<verifyCallback> _pendingBatchCallbacks, void* valid_array){

  int* valid = (int*) valid_array;
  // int valid_size = sizeof(valid) / sizeof(valid[0]);
  //int cb_size = _pendingBatchCallbacks.size();
  // UW_ASSERT(cb_size == valid_size);

  //currently need to call the callbacks for failure results too. If one keeps a global datastructure instead of
  //a dynamic verificationObj then one would "know" if is deleted already or not.
  Debug("Call manageCallbacks for %d items", _pendingBatchCallbacks.size());
  for (int i = 0; i < _pendingBatchCallbacks.size(); ++i){
      bool* res = new bool;
      if(valid[i]){
        std::string hashStr(*_signaturesS[i]);
        std::string rootSig(*_messagesS[i]);
        cache[hashStr] = rootSig;
        bool* res = new bool(true);
        _pendingBatchCallbacks[i]((void*) res);
      }
      else{
        bool* res = new bool(false);
        _pendingBatchCallbacks[i]((void*) res);
      }
  }
  delete [] valid;
  for(int i =0; i< _messagesS.size(); ++i){
    delete _messagesS[i];
    delete _signaturesS[i];
  }
}

void LocalBatchVerifier::AdjustBatchSize() {
  batch_size = (0.75 * batch_size) + (0.25 * messagesBatchedInterval);
  messagesBatchedInterval = 0;
  transport->TimerMicro(batchTimeoutMicro, std::bind(&LocalBatchVerifier::AdjustBatchSize,
        this));
}

} // namespace indicusstore
