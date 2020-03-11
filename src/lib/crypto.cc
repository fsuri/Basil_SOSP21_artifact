#include "lib/crypto.h"

namespace crypto {

using namespace CryptoPP;
using namespace std;

using Signer = ECDSA<ECP, SHA256>::Signer;
using Verifier = ECDSA<ECP, SHA256>::Verifier;

string Hash(const string &message) {
  SHA256 hash;

  std::string digest;

  StringSource s(message, true, new HashFilter(hash, new StringSink(digest)));

  return digest;
}

string Sign(const PrivKey &privateKey, const string &message) {
  // sign message
  std::string signature;
  Signer signer(privateKey);
  AutoSeededRandomPool prng;

  StringSource ss(message, true,
                  new SignerFilter(prng, signer, new StringSink(signature)));

  return signature;
}

bool Verify(const PubKey &publicKey, const string &message, string &signature) {
  // verify message
  bool result = false;
  Verifier verifier(publicKey);
  StringSource ss2(
      signature + message, true,
      new SignatureVerificationFilter(
          verifier, new ArraySink((uint8_t *)&result, sizeof(result))));

  return result;
}

void Save(const std::string &filename, const BufferedTransformation &bt) {
  FileSink file(filename.c_str());

  bt.CopyTo(file);
  file.MessageEnd();
}

void SavePublicKey(const string &filename, PubKey &key) {
  ByteQueue queue;
  key.Save(queue);

  Save(filename, queue);
}

void SavePrivateKey(const std::string &filename, PrivKey &key) {
  ByteQueue queue;
  key.Save(queue);

  Save(filename, queue);
}

void Load(const string &filename, BufferedTransformation &bt) {
  FileSource file(filename.c_str(), true /*pumpAll*/);

  file.TransferTo(bt);
  bt.MessageEnd();
}

PubKey LoadPublicKey(const string &filename) {
  PubKey key;
  ByteQueue queue;
  Load(filename, queue);

  key.Load(queue);

  return key;
}

PrivKey LoadPrivateKey(const string &filename) {
  PrivKey key;
  ByteQueue queue;
  Load(filename, queue);

  key.Load(queue);

  return key;
}

PrivKey GeneratePrivateKey() {
  // PGP Random Pool-like generator
  AutoSeededRandomPool prng;

  // generate keys
  ECDSA<ECP, SHA256>::PrivateKey privateKey;
  privateKey.Initialize(prng, ASN1::secp256k1());

  return privateKey;
}

PubKey DerivePublicKey(PrivKey &privateKey) {
  // PGP Random Pool-like generator
  AutoSeededRandomPool prng;

  ECDSA<ECP, SHA256>::PublicKey publicKey;
  privateKey.MakePublicKey(publicKey);

  bool result = publicKey.Validate(prng, 3);
  if (!result) {
    throw "Public key derivation failed";
  }

  return publicKey;
}

}  // namespace crypto