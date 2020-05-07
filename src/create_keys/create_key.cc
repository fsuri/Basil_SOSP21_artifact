#include <iostream>
#include <string>

#include "lib/crypto.h"

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cout << "Usage ./create_key <keyname>" << std::endl;
    return 1;
  }

  const char* keyname = argv[1];
  std::string keyFileName(keyname);

  #ifdef USE_ED25519_SIGS
  std::pair<crypto::PrivKey, crypto::PubKey> keypair = crypto::GenerateKeypair();
  crypto::PrivKey privKey = keypair.first;
  crypto::PubKey pubKey = keypair.second;
  #else
  crypto::PrivKey privKey = crypto::GeneratePrivateKey();
  crypto::PubKey pubKey = crypto::DerivePublicKey(privKey);
  #endif

  std::string privateKeyname = keyFileName + ".priv";
  crypto::SavePrivateKey(privateKeyname, privKey);
  std::cout << "Saved private key to: " << privateKeyname << std::endl;

  std::string publicKeyname = keyFileName + ".pub";
  crypto::SavePublicKey(publicKeyname, pubKey);
  std::cout << "Saved public key to: " << publicKeyname << std::endl;
}
