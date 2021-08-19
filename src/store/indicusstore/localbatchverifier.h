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
#ifndef LOCAL_BATCH_VERIFIER_H
#define LOCAL_BATCH_VERIFIER_H

#include "store/indicusstore/verifier.h"
#include "store/indicusstore/localbatchverifier.h"
#include "store/common/stats.h"
#include "store/indicusstore/common.h"
#include "lib/latency.h"

#include <mutex>
#include <string>
#include <unordered_map>

namespace indicusstore {

class LocalBatchVerifier : public Verifier {
 public:
  LocalBatchVerifier(uint64_t merkleBranchFactor, Stats &stats, Transport* transport);
  LocalBatchVerifier(uint64_t merkleBranchFactor, Stats &stats, Transport* transport,
    uint64_t batchTimeoutMicro, bool adjustBatchSize, uint64_t batch_size);
  virtual ~LocalBatchVerifier();

  virtual bool Verify2(crypto::PubKey *publicKey, const std::string *message,
      const std::string *signature) override;
  virtual bool Verify(crypto::PubKey *publicKey, const std::string &message,
      const std::string &signature) override;

  // virtual void AddToBatch(crypto::PubKey *publicKey, const std::string &message,
  //             const std::string &signature);  //add callback argument
  //
  // virtual bool VerifyBatch(int *valid);  //change it so return value is the pointer.

  //AsyncBatching functions

  virtual void asyncBatchVerify(crypto::PubKey *publicKey, const std::string &message,
      const std::string &signature, verifyCallback vb, bool multithread, bool autocomplete = false) override;

  virtual void Complete(bool multithread, bool force_complete = false) override;



 private:
  std::mutex cacheMutex;
  Transport *transport;
  const uint64_t merkleBranchFactor;
  Stats &stats;
  std::vector<Latency_t> hashLats;
  std::vector<Latency_t> cryptoLats;
  std::unordered_map<std::string, std::string> cache;

  bool batchTimerRunning;
  uint64_t batch_size;
  uint64_t messagesBatchedInterval;
  const unsigned int batchTimeoutMicro;
  int batchTimerId;

  //add vectors for Batching
  static const int max_fill = 64;
  int current_fill = 0;
  std::vector<crypto::PubKey*> publicKeys;
  std::vector<const char*> messages;
  std::vector<size_t> messageLens;
  std::vector<const char*> signatures;

  std::vector<std::string*> signaturesS;
  std::vector<std::string*> messagesS;

  std::vector<verifyCallback> pendingBatchCallbacks;

  void asyncBatchVerifyCallback(crypto::PubKey *publicKey, std::string *hashStr,
    std::string *rootSig, verifyCallback vb, bool multithread, bool autocomplete, void* validate);

  void* asyncComputeBatchVerification(std::vector<crypto::PubKey*> _publicKeys,
    std::vector<const char*> _messages, std::vector<size_t> _messageLens, std::vector<const char*> _signatures,
    int _current_fill);

  void* asyncComputeBatchVerificationS(std::vector<crypto::PubKey*> _publicKeys,
      std::vector<std::string*> _messagesS, std::vector<size_t> _messageLens,
      std::vector<std::string*> _signaturesS, int _current_fill);


  void manageCallbacks(std::vector<const char*> &_messages, std::vector<const char*> &_signatures,
       std::vector<verifyCallback> &_pendingBatchCallbacks, void* valid_array);

  void manageCallbacksS(std::vector<std::string*> &_messages, std::vector<std::string*> &_signatures,
       std::vector<verifyCallback> &_pendingBatchCallbacks, void* valid_array);

  void AdjustBatchSize();

  bool partialVerify(crypto::PubKey *publicKey, const std::string &hashStr, const std::string &rootSig);


};

} // namespace indicusstore

#endif /* LOCAL_BATCH_VERIFIER_H */
