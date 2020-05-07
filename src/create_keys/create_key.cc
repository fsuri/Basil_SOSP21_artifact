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
