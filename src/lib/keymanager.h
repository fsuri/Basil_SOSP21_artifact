#ifndef LIB_KEYMANAGER_H
#define LIB_KEYMANAGER_H

#include "lib/crypto.h"

#include <map>

class KeyManager {
 public:
  KeyManager(const std::string &keyPath);
  virtual ~KeyManager();

  const crypto::PubKey &GetPublicKey(uint64_t id);
  const crypto::PrivKey &GetPrivateKey(uint64_t id);

 private:
  const std::string keyPath;
  std::map<uint64_t, crypto::PubKey> publicKeys;
  std::map<uint64_t, crypto::PrivKey> privateKeys;
};

#endif
