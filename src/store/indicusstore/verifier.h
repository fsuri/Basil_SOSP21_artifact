#ifndef VERIFIER_H
#define VERIFIER_H

#include <string>

#include "lib/crypto.h"
//#include "lib/crypto.cc"
#include "lib/tcptransport.h"
//#include "store/indicusstore/common.h"

namespace indicusstore {

  typedef std::function<void(void*)> verifyCallback;

  template<typename T> static void* pointerWrapperC(std::function<T()> func){
      T* t = new T; //(T*) malloc(sizeof(T));
      *t = func();
      return (void*) t;
  }

class Verifier {
 public:
  Verifier() { }
  virtual ~Verifier() { }

  virtual bool Verify(crypto::PubKey *publicKey, const std::string &message,
      const std::string &signature) = 0;

};

} // namespace indiucsstore

#endif /* VERIFIER_H */
