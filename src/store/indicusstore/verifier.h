#ifndef VERIFIER_H
#define VERIFIER_H

#include <string>

#include "lib/crypto.h"

namespace indicusstore {

class Verifier {
 public:
  Verifier() { }
  virtual ~Verifier() { }

  virtual bool Verify(crypto::PubKey *publicKey, const std::string &message,
      const std::string &signature) = 0;

};

} // namespace indiucsstore

#endif /* VERIFIER_H */
