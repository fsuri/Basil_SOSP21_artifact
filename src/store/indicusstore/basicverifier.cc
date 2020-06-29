#include "store/indicusstore/basicverifier.h"

#include "lib/crypto.h"
//#include "lib/crypto.cc"
#include "lib/assert.h"

namespace indicusstore {

BasicVerifier::BasicVerifier() {
}

BasicVerifier::~BasicVerifier() {
}

bool BasicVerifier::Verify(crypto::PubKey *publicKey, const std::string &message,
    const std::string &signature) {
  return crypto::Verify(publicKey, &message[0], message.length(), &signature[0]);
}

void BasicVerifier::AddToBatch(crypto::PubKey *publicKey, const std::string &message,
    const std::string &signature){
    //UW_ASSERT(publicKey->t == crypto::KeyType::DONNA);
    UW_ASSERT(max_fill > current_fill);

    publicKeys[current_fill] = publicKey;
    messages[current_fill] = &message[0];
    messageLens[current_fill] = message.length();
    signatures[current_fill] = &signature[0];

    current_fill++;
}

//input to function: 1) int valid[verifier->CurrentFill()] 2) VerifyBatch(&valid[0])
bool BasicVerifier::VerifyBatch(int *valid){
    UW_ASSERT(current_fill>0);
    bool all_valid = crypto::BatchVerify(crypto::KeyType::DONNA, publicKeys, messages, messageLens, signatures, current_fill, valid);
    current_fill = 0;
    return all_valid;
}

} // namespace indicusstore
