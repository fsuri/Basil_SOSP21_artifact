/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fsp@cs.cornell.edu>
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
#include <iostream>
#include <string>

#include "lib/crypto.h"

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cout << "Usage ./create_key <type> <keyname>" << std::endl;
    return 1;
  }

  crypto::KeyType t;
  switch (atoi(argv[1])) {
  case 0:
    t = crypto::RSA;
    break;
  case 1:
    t = crypto::ECDSA;
    break;
  case 2:
    t = crypto::ED25;
    break;
  case 3:
    t = crypto::SECP;
    break;
  case 4:
    t = crypto::DONNA;
    break;
  default:
    throw "unimplemented";
  }

  const char* keyname = argv[2];
  std::string keyFileName(keyname);

  std::pair<crypto::PrivKey*, crypto::PubKey*> keypair = crypto::GenerateKeypair(t, false);
  crypto::PrivKey* privKey = keypair.first;
  crypto::PubKey* pubKey = keypair.second;

  std::string privateKeyname = keyFileName + ".priv";
  crypto::SavePrivateKey(privateKeyname, privKey);
  std::cout << "Saved private key to: " << privateKeyname << std::endl;

  std::string publicKeyname = keyFileName + ".pub";
  crypto::SavePublicKey(publicKeyname, pubKey);
  std::cout << "Saved public key to: " << publicKeyname << std::endl;
}
