#ifndef BASIC_VERIFIER_H
#define BASIC_VERIFIER_H

#include "store/indicusstore/verifier.h"


namespace indicusstore {

class BasicVerifier : public Verifier {

  private:
    static const int max_fill = 64; //max batch size.
    int current_fill;  // Call Batch Verify with this, current index;
    crypto::PubKey* publicKeys[max_fill];
    const char *messages[max_fill];
    size_t messageLens[max_fill];
    const char *signatures[max_fill];

 public:
  BasicVerifier();
  virtual ~BasicVerifier();

  bool Full(){
    return max_fill == current_fill;
  }
  int CurrentFill(){
    return current_fill;
  }

  virtual bool Verify(crypto::PubKey *publicKey, const std::string &message,
      const std::string &signature) override;

void AddToBatch(crypto::PubKey* publicKey, const std::string &message,
          const std::string &signature);
//input to function: 1) int valid[verifier->CurrentFill()] 2) VerifyBatch(&valid[0])
  bool VerifyBatch(int* valid);



};

} // namespace indicusstore

#endif /* BASIC_VERIFIER_H */
