#include "store/indicusstore/basicverifier.h"

#include "lib/crypto.h"
//#include "lib/crypto.cc"
#include "lib/assert.h"

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

bool BasicVerifier::Verify(crypto::PubKey *publicKey, const std::string &message,
    const std::string &signature) {
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
    bool all_valid = crypto::BatchVerify(crypto::KeyType::DONNA, _publicKeys.data(), _messages.data(), _messageLens.data(), _signatures.data(), _current_fill, valid);

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
          std::function<bool()> func(std::bind(&Verifier::Verify, this, publicKey, message, signature));
          std::function<void*()> f(std::bind(pointerWrapperC<bool>, func));
          transport->DispatchTP(f, vb);
        }
        else{
          std::function<bool()> func(std::bind(&Verifier::Verify, this, publicKey, message, signature));
          //Verify(publicKey, message, signature)
          void* res = pointerWrapperC<bool>(func);
          vb(res);
        }
      }

    else {
      UW_ASSERT(publicKey->t == crypto::KeyType::DONNA);

      messagesBatchedInterval++;
      publicKeys.push_back(publicKey);
      messages.push_back(&message[0]);
      messageLens.push_back(message.length());
      signatures.push_back(&signature[0]);
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

void BasicVerifier::Complete(bool multithread, bool force_complete){

  if(force_complete || current_fill >= batch_size) {

    if (batchTimerRunning) {
      transport->CancelTimer(batchTimerId);
      batchTimerRunning = false;
    }

    if(multithread){
      std::function<void*()> f(std::bind(&BasicVerifier::asyncComputeBatchVerification, this, publicKeys, messages, messageLens, signatures, current_fill));
      std::function<void(void*)> cb(std::bind(&BasicVerifier::manageCallbacks, this, pendingBatchCallbacks, std::placeholders::_1));
      transport->DispatchTP(f, cb);
    }
    else{
      //cb(f()); //if one wants to bind always this line suffices
      //if trying to avoid the copying from binding, call with args:
      void* valid = asyncComputeBatchVerification(publicKeys, messages, messageLens, signatures, current_fill);
      manageCallbacks(pendingBatchCallbacks, valid);
    }
    publicKeys.clear();
    messages.clear();
    messageLens.clear();
    signatures.clear();
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


//bind the first argument
void BasicVerifier::manageCallbacks(std::vector<verifyCallback> _pendingBatchCallbacks, void* valid_array){

  int* valid = (int*) valid_array;
  int valid_size = sizeof(valid) / sizeof(valid[0]);
  int cb_size = _pendingBatchCallbacks.size();
  UW_ASSERT(cb_size == valid_size);

  //currently need to call the callbacks for failure results too. If one keeps a global datastructure instead of
  //a dynamic verificationObj then one would "know" if is deleted already or not.
  for (int i = 0, size = _pendingBatchCallbacks.size(); i < size; ++i){
      bool* res = new bool;
      if(valid[i]){
        *res = true;
        _pendingBatchCallbacks[i]((void*) res);
      }
      else{
        *res = false;
        _pendingBatchCallbacks[i]((void*) res);
      }
  }
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
