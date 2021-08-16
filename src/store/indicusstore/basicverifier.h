/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fs435@cornell.edu>
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

  virtual bool Verify2(crypto::PubKey *publicKey, const std::string *message,
      const std::string *signature) override;

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

      //if this doesnt work, undo the ref &
  void manageCallbacks(std::vector<verifyCallback> &_pendingBatchCallbacks, void* valid_array);

  void AdjustBatchSize();

};

} // namespace indicusstore

#endif /* BASIC_VERIFIER_H */
