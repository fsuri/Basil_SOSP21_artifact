#include "lib/crypto.h"
#include "lib/assert.h"

namespace crypto {

using namespace CryptoPP;
using namespace std;

#ifdef USE_ECDSA_SIGS
// using Signer = ECDSA<ECP, SHA256>::Signer;
// using Verifier = ECDSA<ECP, SHA256>::Verifier;
#else
using Signer = RSASS<PSS, SHA256>::Signer;
using Verifier = RSASS<PSS, SHA256>::Verifier;
#endif

string Hash(const string &message) {
  SHA256 hash;

  std::string digest;

  StringSource s(message, true, new HashFilter(hash, new StringSink(digest)));

  return digest;
}

string Sign(const PrivKey &privateKey, const string &message) {
  #ifdef USE_ECDSA_SIGS
  unsigned char edsig[crypto_sign_BYTES];
  crypto_sign_detached(edsig, NULL, (const unsigned char*) message.c_str(), message.length(), privateKey);
  std::string signature(reinterpret_cast<char*>(edsig), crypto_sign_BYTES);
  #else
  // sign message
  std::string signature;
  Signer signer(privateKey);
  AutoSeededRandomPool prng;

  StringSource ss(message, true,
                  new SignerFilter(prng, signer, new StringSink(signature)));
  #endif

  return signature;
}

bool Verify(const PubKey &publicKey, const string &message, const string &signature) {
  #ifdef USE_ECDSA_SIGS
  bool result = crypto_sign_verify_detached((const unsigned char*) signature.c_str(), (const unsigned char*) message.c_str(), message.length(), publicKey) == 0;
  #else
  // verify message
  bool result = false;
  Verifier verifier(publicKey);
  StringSource ss2(
      signature + message, true,
      new SignatureVerificationFilter(
          verifier, new ArraySink((uint8_t *)&result, sizeof(result))));
  #endif

  return result;
}

void Save(const std::string &filename, const BufferedTransformation &bt) {
  FileSink file(filename.c_str());

  bt.CopyTo(file);
  file.MessageEnd();
}

void SavePublicKey(const string &filename, PubKey &key) {
  #ifdef USE_ECDSA_SIGS
  FILE * file = fopen(filename.c_str(), "w+");
  fwrite(key, sizeof(unsigned char), crypto_sign_PUBLICKEYBYTES, file);
  fclose(file);
  #else
  ByteQueue queue;
  key.Save(queue);

  Save(filename, queue);
  #endif
}

void SavePrivateKey(const std::string &filename, PrivKey &key) {
  #ifdef USE_ECDSA_SIGS
  FILE * file = fopen(filename.c_str(), "w+");
  fwrite(key, sizeof(unsigned char), crypto_sign_SECRETKEYBYTES, file);
  fclose(file);
  #else
  ByteQueue queue;
  key.Save(queue);

  Save(filename, queue);
  #endif
}

void Load(const string &filename, BufferedTransformation &bt) {
  FileSource file(filename.c_str(), true /*pumpAll*/);

  file.TransferTo(bt);
  bt.MessageEnd();
}

PubKey LoadPublicKey(const string &filename) {
  PubKey key;
  #ifdef USE_ECDSA_SIGS
  FILE * file = fopen(filename.c_str(), "r+");
  key = (PubKey) malloc(crypto_sign_PUBLICKEYBYTES);
  fread(key, sizeof(unsigned char), crypto_sign_PUBLICKEYBYTES, file);
  fclose(file);
  #else
  ByteQueue queue;
  Load(filename, queue);

  key.Load(queue);
  #endif

  return key;
}

PrivKey LoadPrivateKey(const string &filename) {
  PrivKey key;
  #ifdef USE_ECDSA_SIGS
  // Reading data to array of unsigned chars
  FILE * file = fopen(filename.c_str(), "r+");
  key = (PrivKey) malloc(crypto_sign_SECRETKEYBYTES);
  fread(key, sizeof(unsigned char), crypto_sign_SECRETKEYBYTES, file);
  fclose(file);
  #else
  ByteQueue queue;
  Load(filename, queue);

  key.Load(queue);
  #endif

  return key;
}

PrivKey GeneratePrivateKey() {
  // PGP Random Pool-like generator
  AutoSeededRandomPool prng;

  // generate keys
  PrivKey privateKey;
  #ifdef USE_ECDSA_SIGS
  // privateKey.Initialize(prng, ASN1::secp256k1());
  Panic("Illegal");
  #else
  privateKey.Initialize(prng, 2048);
  #endif

  return privateKey;
}

PubKey DerivePublicKey(PrivKey &privateKey) {
  // PGP Random Pool-like generator
  AutoSeededRandomPool prng;

  #ifdef USE_ECDSA_SIGS
  PubKey publicKey;
  // privateKey.MakePublicKey(publicKey);
  Panic("Illegal");
  #else
  PubKey publicKey(privateKey);
  bool result = publicKey.Validate(prng, 3);
  if (!result) {
    throw "Public key derivation failed";
  }
  #endif


  return publicKey;
}

std::pair<PrivKey, PubKey> GenerateKeypair() {
  PubKey pk = (PubKey) malloc(crypto_sign_PUBLICKEYBYTES);
  PrivKey sk = (PrivKey) malloc(crypto_sign_SECRETKEYBYTES);
  crypto_sign_keypair(pk, sk);
  return std::pair<PrivKey, PubKey>(sk, pk);
}

}  // namespace crypto
