/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fsp@cs.cornell.edu>
 *                Matthew Burke <matthelb@cs.cornell.edu>
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
#include "store/indicusstore/basicverifier.h"

#include "lib/crypto.h"
//#include "lib/crypto.cc"
#include "lib/assert.h"
#include "store/indicusstore/common.h"

namespace indicusstore {

BasicVerifier::BasicVerifier(Transport* transport) : transport(transport), batchTimeoutMicro(ULONG_MAX){
}

BasicVerifier::BasicVerifier(Transport* transport, uint64_t batchTimeoutMicro, bool adjustBatchSize, uint64_t batch_size) : transport(transport),
  batchTimerRunning(false), batch_size(batch_size), messagesBatchedInterval(0UL), batchTimeoutMicro(batchTimeoutMicro) {
    if (adjustBatchSize) {
      transport->TimerMicro(batchTimeoutMicro, std::bind(
          &BasicVerifier::AdjustBatchSize, this));
    }
}

BasicVerifier::~BasicVerifier() {
}

bool BasicVerifier::Verify2(crypto::PubKey *publicKey, const std::string *message,
    const std::string *signature) {
  return Verify(publicKey, *message, *signature);
}

bool BasicVerifier::Verify(crypto::PubKey *publicKey, const std::string &message,
    const std::string &signature) {
      Debug("VERIFYING ON THIS cpu: %d",  sched_getcpu());
  return crypto::Verify(publicKey, &message[0], message.length(), &signature[0]);
}

//TODO: need to make this thread safe if being called from multiple threads. Depends on whether the call into verifier is made by original process or delegated thread. Delegated thread makes more sne
void BasicVerifier::AddToBatch(crypto::PubKey *publicKey, const std::string &message,
    const std::string &signature){
    //UW_ASSERT(publicKey->t == crypto::KeyType::DONNA);
    UW_ASSERT(max_fill > current_fill);

    publicKeys.push_back(publicKey);
    messages.push_back(&message[0]);
    messageLens.push_back(message.length());
    signatures.push_back(&signature[0]);

    current_fill++;
}

//In a no threading env: ValidateP1Replies just calls AddToBatch everytime and then calls VerifyBatch at the end.
//input to function: 1) int valid[verifier->CurrentFill()] 2) VerifyBatch(&valid[0])
bool BasicVerifier::VerifyBatch(int *valid){
    UW_ASSERT(current_fill>0);
    bool all_valid = crypto::BatchVerify(crypto::KeyType::DONNA, publicKeys.data(), messages.data(), messageLens.data(), signatures.data(), current_fill, valid);

    return all_valid;
}

void* BasicVerifier::asyncComputeBatchVerification(std::vector<crypto::PubKey*> _publicKeys,
  std::vector<const char*> _messages, std::vector<size_t> _messageLens,
  std::vector<const char*> _signatures, int _current_fill){
    UW_ASSERT(_current_fill>0);
    UW_ASSERT(_publicKeys[0]->t == crypto::KeyType::DONNA);
    int* valid = new int[_current_fill];
    Debug("BatchVerifying %d items", _current_fill);
    // for(int i=0; i<_current_fill; ++i){
    //   Debug("signature %d: %s ", i, BytesToHex(_signatures[i], 1024).c_str());
    //   Debug("message %d: %s ", i, BytesToHex(_messages[i], 1024).c_str());
    //   //Debug("public_key %d: %s ", i, BytesToHex(*_publicKeys[i], 1024).c_str());
    // }
    bool all_valid = crypto::BatchVerify(crypto::KeyType::DONNA, _publicKeys.data(), _messages.data(),
        _messageLens.data(), _signatures.data(), _current_fill, valid);
    Debug("All valid: %s", all_valid ? "true" : "false");
    return (void*) valid;
}

void* BasicVerifier::asyncComputeBatchVerificationS(std::vector<crypto::PubKey*> _publicKeys,
  std::vector<std::string*> _messages, std::vector<size_t> _messageLens,
  std::vector<std::string*> _signatures, int _current_fill){
    UW_ASSERT(_current_fill>0);
    UW_ASSERT(_publicKeys[0]->t == crypto::KeyType::DONNA);
    int* valid = new int[_current_fill];
    Debug("BatchVerifying %d items", _current_fill);

    bool all_valid = crypto::BatchVerifyS(crypto::KeyType::DONNA, _publicKeys.data(), _messages.data(),
        _messageLens.data(), _signatures.data(), _current_fill, valid);
    Debug("All valid: %s", all_valid ? "true" : "false");
    for(int i =0; i<_current_fill; ++i){
      //free((void*)messages[i]);
      //free((void*)signatures[i]);
      delete _messages[i];
      delete _signatures[i];
    }
    return (void*) valid;
}


//correct the multithreading function dispatching.

void BasicVerifier::asyncBatchVerify(crypto::PubKey *publicKey, const std::string &message,
    const std::string &signature, verifyCallback vb, bool multithread, bool autocomplete){
      //autocomplete param indicates whether complete should be called as soon as a batch is full
      //Allows us to ignore batch limit and batch all together.

//TODO: add initialBatchSize
    if (batch_size == 1) {
        Debug("Initial batch size = 1, immediately verifying");

        //TODO: add dispatching
        if(multithread){
          //std::string msg(message);
          //std::string sig(signature);
          std::function<bool()> func(std::bind(&Verifier::Verify, this, publicKey, message, signature));
          //std::function<void*()> f(std::bind(pointerWrapperC<bool>, std::move(func)));
          std::function<void*()> f(std::bind(BoolPointerWrapper, std::move(func)));
          transport->DispatchTP(std::move(f), std::move(vb));
        }
        else{
          // std::function<bool()> func(std::bind(&Verifier::Verify, this, publicKey, message, signature));
          // //Verify(publicKey, message, signature)
          // void* res = pointerWrapperC<bool>(func);
          // vb(res);
          //bool* res = new bool(Verify(publicKey, message, signature));
          vb((void*) Verify(publicKey, message, signature));
        }
      }

    else {
      UW_ASSERT(publicKey->t == crypto::KeyType::DONNA);

      messagesBatchedInterval++;

      //Use this if using char* vector interface:
      // char* msg = (char*) malloc(message.length());
      // memcpy(msg, &message[0], message.length());
      //
      // char* sig = (char*) malloc(signature.length());
      // memcpy(sig, &signature[0], signature.length());

      std::string* msg_copy = new std::string(message);
      std::string* sig_copy = new std::string(signature);

      messagesS.push_back(msg_copy);
      signaturesS.push_back(sig_copy);

      publicKeys.push_back(publicKey);
      //messages.push_back(msg);
      messageLens.push_back(message.length());
      //signatures.push_back(sig);
      pendingBatchCallbacks.push_back(std::move(vb));
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

void BasicVerifier::Complete(bool multithread, bool force_complete){


  Debug("TRYING TO CALL COMPLETE WITH FILL: %d", current_fill);
  //UW_ASSERT(current_fill>0);

  if(current_fill == 0) return;
  if(force_complete || batch_size >= 1) {


    if (batchTimerRunning) {
      transport->CancelTimer(batchTimerId);
      batchTimerRunning = false;
    }

    if(multithread){
      // std::function<void*()> f(std::bind(&BasicVerifier::asyncComputeBatchVerification, this, publicKeys,
      //   messages, messageLens, signatures, current_fill));

      //TODO: FIX Problem: currently copying the pointers, and not the contents.
      //copy all data.
      // std::vector<crypto::PubKey*> publicKeys;
      // std::vector<const char*> messages;
      // std::vector<size_t> messageLens;
      // std::vector<const char*> signatures;
      // std::vector<verifyCallback> pendingBatchCallbacks;
      //
      // std::vector<std::string*> signaturesS;
      // std::vector<std::string*> messagesS;

      Debug("DISPATCHING BATCH VERIFICATION WITH FILL: %d", current_fill);
      //clearing the data needs to be done in the functions then.
      std::function<void*()> f(std::bind(&BasicVerifier::asyncComputeBatchVerificationS, this, publicKeys,
          messagesS, messageLens, signaturesS, current_fill));
      std::function<void(void*)> cb(std::bind(&BasicVerifier::manageCallbacks, this, std::move(pendingBatchCallbacks), std::placeholders::_1));
      transport->DispatchTP(std::move(f), std::move(cb));
    }
    else{
      //cb(f()); //if one wants to bind always this line suffices
      //if trying to avoid the copying from binding, call with args:
      Debug("TRYING TO CALL BATCH VERIFICATION WITH FILL: %d", current_fill);
      //void* valid = asyncComputeBatchVerification(publicKeys, messages, messageLens, signatures, current_fill);
      void* valid = asyncComputeBatchVerificationS(publicKeys, messagesS, messageLens, signaturesS, current_fill);
      manageCallbacks(pendingBatchCallbacks, valid);
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
    //Complete(multithread, true); //this works
    //Sanity Test
     // auto lambda = [this, multithread](){this->batchTimerRunning = false; this->Complete(multithread, true); };
     // //lambda(); //this works
     //  batchTimerId = transport->TimerMicro(0, lambda); //this does not

    if(current_fill == 0) {Debug("NO FILL, DONT START TIMER"); return;}
    batchTimerRunning = true;
    Debug("Starting batch timer");
    batchTimerId = transport->TimerMicro(batchTimeoutMicro, [this, multithread]() {
      Debug("Batch timer expired with %d items, verifying",
          this->current_fill);
      this->batchTimerRunning = false;
      this->Complete(multithread, true); //SOMEHOW THIS makes the result go false
    });

    // // Debug("TIMEOUT: TRYING TO CALL BATCH VERIFICATION WITH FILL: %d", this->current_fill);
    // // int* valid = new int[this->current_fill];
    // // Debug("BatchVerifying %d items", this->current_fill);
    // // bool all_valid = crypto::BatchVerify(crypto::KeyType::DONNA, publicKeys.data(), messages.data(), messageLens.data(), signatures.data(), current_fill, valid);
    // // Debug("All valid: %s", all_valid ? "true" : "false");
    // // this->manageCallbacks(pendingBatchCallbacks, valid);
    // // publicKeys.clear();
    // // messages.clear();
    // // messageLens.clear();
    // // signatures.clear();
    // // pendingBatchCallbacks.clear();
    // // current_fill = 0;
    // });
  }
}


//bind the first argument
void BasicVerifier::manageCallbacks(std::vector<verifyCallback> &_pendingBatchCallbacks, void* valid_array){

  int* valid = (int*) valid_array;
  // int valid_size = sizeof(valid) / sizeof(valid[0]);
  //int cb_size = _pendingBatchCallbacks.size();
  // Debug("sizeof(valid): %d, sizeof(valid[0]): %d", sizeof(valid), sizeof(valid[0]));
  // Debug("cb_size: %d, valid_size: %d", cb_size, valid_size);
  // UW_ASSERT(cb_size == valid_size);

  //currently need to call the callbacks for failure results too. If one keeps a global datastructure instead of
  //a dynamic verificationObj then one would "know" if is deleted already or not.

  Debug("Call manageCallbacks for %d items", _pendingBatchCallbacks.size());
  for (int i = 0; i < _pendingBatchCallbacks.size(); ++i){

      //Debug("valid[%d]: %d ", i, valid[i]);
      if(valid[i]){
      //  bool* res = new bool(true);
        _pendingBatchCallbacks[i]((void*) true);
      }
      else{
        //bool* res = new bool(false);
        _pendingBatchCallbacks[i]((void*) false);
      }
  }
  Debug("manageCallbacks complete");
  delete [] valid;
//   for( auto it = pendingBatchCallbacks.begin() ; it != myvector.end(); ++it) :
//     if valid[] : call the callback.
// }
}

void BasicVerifier::AdjustBatchSize() {
  batch_size = (0.75 * batch_size) + (0.25 * messagesBatchedInterval);
  messagesBatchedInterval = 0;
  transport->TimerMicro(batchTimeoutMicro, std::bind(&BasicVerifier::AdjustBatchSize,
        this));
}

} // namespace indicusstore
