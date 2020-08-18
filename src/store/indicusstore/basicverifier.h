#ifndef BASIC_VERIFIER_H
#define BASIC_VERIFIER_H

#include <vector>
#include <mutex>
#include "store/indicusstore/verifier.h"


namespace indicusstore {


class BasicVerifier : public Verifier {

 public:
  BasicVerifier(Transport *transport);
  BasicVerifier(Transport *transport, uint64_t batchTimeoutMicro, bool adjustBatchSize, uint64_t batch_size);
  virtual ~BasicVerifier();

  bool Full(){
    return max_fill == current_fill;
  }
  int CurrentFill(){
    return current_fill;
  }

  virtual bool Verify(crypto::PubKey *publicKey, const std::string &message,
      const std::string &signature) override;

//deprecated
  void AddToBatch(crypto::PubKey *publicKey, const std::string &message,
          const std::string &signature);
//deprecated
  bool VerifyBatch(int *valid);

//AsyncBatching functions
  virtual void asyncBatchVerify(crypto::PubKey *publicKey, const std::string &message,
      const std::string &signature, verifyCallback vb, bool multithread, bool autocomplete = false) override;

  virtual void Complete(bool multithread, bool force_complete = false) override;

private:
  Transport* transport;
  static const int max_fill = 64; //max batch size.
  int current_fill = 0;  // Call Batch Verify with this, current index;


  bool batchTimerRunning;
  uint64_t batch_size;
  uint64_t messagesBatchedInterval;
  const unsigned int batchTimeoutMicro;
  int batchTimerId;

  std::vector<crypto::PubKey*> publicKeys;
  std::vector<const char*> messages;
  std::vector<size_t> messageLens;
  std::vector<const char*> signatures;
  std::vector<verifyCallback> pendingBatchCallbacks;

  std::vector<std::string*> signaturesS;
  std::vector<std::string*> messagesS;




  void* asyncComputeBatchVerification(std::vector<crypto::PubKey*> _publicKeys,
    std::vector<const char*> _messages, std::vector<size_t> _messageLens, std::vector<const char*> _signatures,
    int _current_fill);

    void* asyncComputeBatchVerificationS(std::vector<crypto::PubKey*> _publicKeys,
      std::vector<std::string*> _messages, std::vector<size_t> _messageLens,
      std::vector<std::string*> _signatures, int _current_fill);

  void manageCallbacks(std::vector<verifyCallback> _pendingBatchCallbacks, void* valid_array);

  void AdjustBatchSize();

};

} // namespace indicusstore

#endif /* BASIC_VERIFIER_H */
