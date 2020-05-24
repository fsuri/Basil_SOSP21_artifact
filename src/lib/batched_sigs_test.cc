#include "lib/batched_sigs.h"
#include "lib/assert.h"

#include <string>
#include <vector>
#include <iostream>

int main(int argc, char *argv[]) {
  std::pair<crypto::PrivKey*, crypto::PubKey*> keypair = crypto::GenerateKeypair(crypto::SECP, false);
  crypto::PrivKey* privKey = keypair.first;
  crypto::PubKey* pubKey = keypair.second;

  for (int m = 2; m < 16; ++m) {
  for (int k = 1; k < 64; ++k) {
    std::vector<const std::string *> messages;
    for (int i = 0; i < k; ++i) {
      messages.push_back(new std::string("abcdefghijklmnopqrstuvwxyz123456"));
    }
    std::vector<std::string> sigs;
    std::cerr << "============MERKLE CREATE============ " << k << std::endl;
    BatchedSigs::generateBatchedSignatures(messages, privKey, sigs, m);
    for (size_t i = 0; i < messages.size(); ++i) {
      std::string hashStr;
      std::string rootSig;
      std::cerr << "=========MERKLE VERIFY============ " << i << std::endl;
      UW_ASSERT(BatchedSigs::computeBatchedSignatureHash(&sigs[i], messages[i], pubKey,
          hashStr, rootSig, m));
      UW_ASSERT(crypto::Verify(pubKey, &hashStr[0], hashStr.length(),
            &rootSig[0]));
    }
    for (auto msg : messages) {
      delete msg;
    }
  }
  }
  return 0;
}
