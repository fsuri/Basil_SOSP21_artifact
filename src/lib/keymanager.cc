#include "lib/keymanager.h"

#include <string>

KeyManager::KeyManager(const std::string &keyPath, crypto::KeyType t, bool precompute) :
  keyPath(keyPath), keyType(t), precompute(precompute) {
}

KeyManager::~KeyManager() {
}

crypto::PubKey* KeyManager::GetPublicKey(uint64_t id) {
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
