/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fs435@cornell.edu>
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

  virtual bool Verify2(crypto::PubKey *publicKey, const std::string *message,
      const std::string *signature) = 0;

  virtual bool Verify(crypto::PubKey *publicKey, const std::string &message,
      const std::string &signature) = 0;

  virtual void asyncBatchVerify(crypto::PubKey *publicKey, const std::string &message,
      const std::string &signature, verifyCallback vb, bool multithread, bool autocomplete = false) = 0;

  virtual void Complete(bool multithread, bool force_complete = false) = 0;
};

} // namespace indiucsstore

#endif /* VERIFIER_H */
