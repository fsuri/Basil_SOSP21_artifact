#ifndef LIB_KEYMANAGER_H
#define LIB_KEYMANAGER_H

#include "lib/crypto.h"

#include <map>
#include <mutex>

class KeyManager {
 public:
  KeyManager(const std::string &keyPath, crypto::KeyType t, bool precompute);
  virtual ~KeyManager();

  crypto::PubKey* GetPublicKey(uint64_t id);
  crypto::PrivKey* GetPrivateKey(uint64_t id);

 private:
  const std::string keyPath;
  const crypto::KeyType keyType;
  const bool precompute;
  std::map<uint64_t, crypto::PubKey*> publicKeys;
  std::map<uint64_t, crypto::PrivKey*> privateKeys;
  std::mutex keyMutex;
};

#endif
