#ifndef _LIB_BATCHED_STABLE_SIGS_H_
#define _LIB_BATCHED_STABLE_SIGS_H_

#include <vector>
#include <string>
#include "lib/crypto.h"

namespace bftsmartBatchedSigs_stable {

void generateBatchedSignatures(std::vector<std::string*> messages, crypto::PrivKey* privateKey, std::vector<std::string*> sigs);

bool verifyBatchedSignature(const std::string* signature, const std::string* message, crypto::PubKey* publicKey);

}

#endif
