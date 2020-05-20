#include "store/indicusstore/basicverifier.h"

#include "lib/crypto.h"

namespace indicusstore {

BasicVerifier::BasicVerifier() {
}

BasicVerifier::~BasicVerifier() {
}

bool BasicVerifier::Verify(crypto::PubKey *publicKey, const std::string &message,
    const std::string &signature) {
  return crypto::Verify(publicKey, message, signature);
}

} // namespace indicusstore
