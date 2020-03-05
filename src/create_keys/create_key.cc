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

  crypto::PrivKey privateKey = crypto::GeneratePrivateKey();
  crypto::PubKey publicKey =
      crypto::DerivePublicKey(privateKey);

  std::string privateKeyname = keyFileName + ".priv";
  crypto::SavePrivateKey(privateKeyname, privateKey);
  std::cout << "Saved private key to: " << privateKeyname << std::endl;

  std::string publicKeyname = keyFileName + ".pub";
  crypto::SavePublicKey(publicKeyname, publicKey);
  std::cout << "Saved public key to: " << publicKeyname << std::endl;
}