#ifndef _LIB_BATCHED_SIGS_H_
#define _LIB_BATCHED_SIGS_H_

#include <vector>
#include <string>
#include "lib/crypto.h"

namespace BatchedSigs {

std::vector<std::string> generateBatchedSignatures(std::vector<std::string> messages, crypto::PrivKey* privateKey);

bool verifyBatchedSignature(std::string signature, std::string message, crypto::PubKey* publicKey);

}

#endif
