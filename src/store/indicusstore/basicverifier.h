#ifndef BASIC_VERIFIER_H
#define BASIC_VERIFIER_H

#include "store/indicusstore/verifier.h"

namespace indicusstore {

class BasicVerifier : public Verifier {
 public:
  BasicVerifier();
  virtual ~BasicVerifier();

  virtual bool Verify(crypto::PubKey *publicKey, const std::string &message,
      const std::string &signature) override;

};

} // namespace indicusstore

#endif /* BASIC_VERIFIER_H */
