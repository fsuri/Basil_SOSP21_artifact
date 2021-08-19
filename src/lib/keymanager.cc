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
#include "lib/keymanager.h"

#include <string>

KeyManager::KeyManager(const std::string &keyPath, crypto::KeyType t, bool precompute) :
  keyPath(keyPath), keyType(t), precompute(precompute) {
}

KeyManager::~KeyManager() {
}

crypto::PubKey* KeyManager::GetPublicKey(uint64_t id) {
  std::unique_lock<std::mutex> lock(keyMutex);
  auto itr = publicKeys.find(id);
  if (itr == publicKeys.end()) {
    crypto::PubKey* publicKey =  crypto::LoadPublicKey(keyPath + "/" +
        std::to_string(id) + ".pub", keyType, precompute);
    auto pairItr = publicKeys.insert(std::make_pair(id, publicKey));
    return pairItr.first->second;
  } else {
    return itr->second;
  }
}

crypto::PrivKey* KeyManager::GetPrivateKey(uint64_t id) {
  std::unique_lock<std::mutex> lock(keyMutex);
  auto itr = privateKeys.find(id);
  if (itr == privateKeys.end()) {
    crypto::PrivKey* privateKey =  crypto::LoadPrivateKey(keyPath + "/" +
        std::to_string(id) + ".priv", keyType, precompute);
    auto pairItr = privateKeys.insert(std::make_pair(id, privateKey));
    return pairItr.first->second;
  } else {
    return itr->second;
  }
}

//Todo add function support for Client keys also. Just add a second key path folder for those keys.
