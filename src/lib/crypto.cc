#include "lib/crypto.h"
#include "lib/assert.h"

#include <sodium.h>
#include <cryptopp/eccrypto.h>
#include <cryptopp/rsa.h>
#include <cryptopp/pssr.h>
#include <cryptopp/files.h>
#include <cryptopp/hex.h>
#include <cryptopp/oids.h>
#include <cryptopp/osrng.h>
#include <cryptopp/sha.h>
#include <cryptopp/hmac.h>
#include <random>
#include "lib/secp256k1.h"
#include "lib/blake3.h"
#include "lib/static_block.h"

namespace crypto {

using namespace CryptoPP;
using namespace std;

secp256k1_context *secpCTX;
// hasher struct
blake3_hasher hasher;

static_block {
  secpCTX = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
}

struct PubKey {
  KeyType t;
  union {
    RSA::PublicKey* rsaKey;
    CryptoPP::ECDSA<ECP, SHA256>::PublicKey* ecdsaKey;
    unsigned char* ed25Key;
    unsigned char* secpKey;
  };
};

struct PrivKey {
  KeyType t;
  union {
    RSA::PrivateKey* rsaKey;
    CryptoPP::ECDSA<ECP, SHA256>::PrivateKey* ecdsaKey;
    unsigned char* ed25Key;
    unsigned char* secpKey;
  };
};

std::string HMAC(std::string message, std::string keystr) {
  SecByteBlock key((const CryptoPP::byte*)keystr.data(), keystr.size());
  try{
    CryptoPP::HMAC< CryptoPP::SHA256 > hmac(key, key.size());

    string mac;
    StringSource ss2(message, true,
        new HashFilter(hmac,
            new StringSink(mac)
        ) // HashFilter
    );
    return mac;
  }
  catch(const CryptoPP::Exception& e) {
      std::cerr << e.what() << std::endl;
      exit(1);
  }
}

bool verifyHMAC(std::string message, std::string mac, std::string keystr) {
  SecByteBlock key((const CryptoPP::byte*)keystr.data(), keystr.size());
  try{
      CryptoPP::HMAC< CryptoPP::SHA256 > hmac(key, key.size());
      const int flags = HashVerificationFilter::THROW_EXCEPTION | HashVerificationFilter::HASH_AT_END;

      StringSource(message + mac, true,
          new HashVerificationFilter(hmac, NULL, flags)
      ); // StringSource

      return true;
  }
  catch(const CryptoPP::Exception& e) {
    return false;
  }
}

string Hash(const string &message) {
  SHA256 hash;

  std::string digest;

  StringSource s(message, true, new HashFilter(hash, new StringSink(digest)));

  return digest;
}

size_t HashSize() {
  SHA256 hash;
  return hash.DigestSize();
}

string Sign(PrivKey* privateKey, const string &message) {
  switch(privateKey->t) {
  case RSA: {
    std::string signature;
    RSASS<PSS, SHA256>::Signer signer(*privateKey->rsaKey);
    AutoSeededRandomPool prng;

    StringSource ss(message, true,
                    new SignerFilter(prng, signer, new StringSink(signature)));
    return signature;
  }
  case ECDSA: {
    std::string signature;
    CryptoPP::ECDSA<ECP, SHA256>::Signer signer(*privateKey->ecdsaKey);
    AutoSeededRandomPool prng;

    StringSource ss(message, true,
                    new SignerFilter(prng, signer, new StringSink(signature)));
    return signature;
  }
  case ED25: {
    std::string signature;
    signature.resize(crypto_sign_BYTES);
    crypto_sign_detached((unsigned char*) &signature[0], NULL, (unsigned char*) &message[0], message.length(), privateKey->ed25Key);
    return signature;
  }
  case SECP: {
    std::string signature;
    signature.resize(64);
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, (unsigned char*) &message[0], message.length());
    unsigned char out[BLAKE3_OUT_LEN];
    blake3_hasher_finalize(&hasher, out, BLAKE3_OUT_LEN);
    secp256k1_ecdsa_sign(secpCTX, (secp256k1_ecdsa_signature*) &signature[0], out, privateKey->secpKey, NULL, NULL);
    return signature;
  }
  default: {
    Panic("unimplemented");
  }
  }
}

size_t SigSize(KeyType t) {
  switch(t) {
  case RSA: {
    Panic("unimplemented");
  }
  case ECDSA: {
    Panic("unimplemented");
  }
  case ED25: {
    return crypto_sign_BYTES;
  }
  case SECP: {
    return sizeof(secp256k1_ecdsa_signature);
  }
  default: {
    Panic("unimplemented");
  }
  }
}

size_t SigSize(PrivKey* privateKey) {
  return SigSize(privateKey->t);
}
size_t SigSize(PubKey* publicKey) {
  return SigSize(publicKey->t);
}


bool Verify(PubKey* publicKey, const string &message, const string &signature) {
  switch(publicKey->t) {
  case RSA: {
    bool result = false;
    RSASS<PSS, SHA256>::Verifier verifier(*publicKey->rsaKey);
    StringSource ss2(
        signature + message, true,
        new SignatureVerificationFilter(
            verifier, new ArraySink((uint8_t *)&result, sizeof(result))));
    return result;
  }
  case ECDSA: {
    bool result = false;
    CryptoPP::ECDSA<ECP, SHA256>::Verifier verifier(*publicKey->ecdsaKey);
    StringSource ss2(
        signature + message, true,
        new SignatureVerificationFilter(
            verifier, new ArraySink((uint8_t *)&result, sizeof(result))));
    return result;
  }
  case ED25: {
    return crypto_sign_verify_detached((unsigned char*) &signature[0], (unsigned char*) &message[0], message.length(), publicKey->ed25Key) == 0;
  }
  case SECP: {
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, (unsigned char*) &message[0], message.length());
    unsigned char out[BLAKE3_OUT_LEN];
    blake3_hasher_finalize(&hasher, out, BLAKE3_OUT_LEN);
    return secp256k1_ecdsa_verify(secpCTX, (secp256k1_ecdsa_signature*) &signature[0], out, (secp256k1_pubkey*) publicKey->secpKey) == 1;
  }

  }
  Panic("unimplemented");
}

void Save(const std::string &filename, const BufferedTransformation &bt) {
  FileSink file(filename.c_str());

  bt.CopyTo(file);
  file.MessageEnd();
}

void SaveCFile(const std::string &filename, unsigned char* bytes, size_t length) {
  FILE * file = fopen(filename.c_str(), "w+");
  fwrite(bytes, sizeof(unsigned char), length, file);
  fclose(file);
}

void SavePublicKey(const string &filename, PubKey* key) {
  switch(key->t) {
  case RSA: {
    ByteQueue queue;
    key->rsaKey->Save(queue);

    Save(filename, queue);
    break;
  }
  case ECDSA: {
    ByteQueue queue;
    key->ecdsaKey->Save(queue);

    Save(filename, queue);
    break;
  }
  case ED25: {
    SaveCFile(filename, key->ed25Key, crypto_sign_PUBLICKEYBYTES);
    break;
  }
  case SECP: {
    SaveCFile(filename, key->secpKey, sizeof(secp256k1_pubkey));
    break;
  }
  default: {
    Panic("unimplemented");
  }
  }
}

void SavePrivateKey(const std::string &filename, PrivKey* key) {
  switch(key->t) {
  case RSA: {
    ByteQueue queue;
    key->rsaKey->Save(queue);

    Save(filename, queue);
    break;
  }
  case ECDSA: {
    ByteQueue queue;
    key->ecdsaKey->Save(queue);

    Save(filename, queue);
    break;
  }
  case ED25: {
    SaveCFile(filename, key->ed25Key, crypto_sign_SECRETKEYBYTES);
    break;
  }
  case SECP: {
    SaveCFile(filename, key->secpKey, 32);
    break;
  }
  default: {
    Panic("unimplemented");
  }
  }
}

void Load(const string &filename, BufferedTransformation &bt) {
  FileSource file(filename.c_str(), true /*pumpAll*/);

  file.TransferTo(bt);
  bt.MessageEnd();
}

void LoadCFile(const std::string &filename, unsigned char* bytes, size_t length) {
  FILE * file = fopen(filename.c_str(), "r");
  if (file == NULL) {
    Panic("Invalid filename %s", filename.c_str());
  }
  fread(bytes, sizeof(unsigned char), length, file);
  fclose(file);
}

PubKey* LoadPublicKey(const string &filename, KeyType t, bool precompute) {
  PubKey* key = (PubKey*) malloc(sizeof(PubKey));
  key->t = t;
  switch(key->t) {
  case RSA: {
    key->rsaKey = new RSA::PublicKey();
    ByteQueue queue;
    Load(filename, queue);

    key->rsaKey->Load(queue);
    break;
  }
  case ECDSA: {
    key->ecdsaKey = new CryptoPP::ECDSA<ECP, SHA256>::PublicKey();
    ByteQueue queue;
    Load(filename, queue);

    key->ecdsaKey->Load(queue);
    if (precompute) {
      key->ecdsaKey->Precompute();
    }
    break;
  }
  case ED25: {
    key->ed25Key = (unsigned char*) malloc(crypto_sign_PUBLICKEYBYTES);
    LoadCFile(filename, key->ed25Key, crypto_sign_PUBLICKEYBYTES);
    break;
  }
  case SECP: {
    key->ed25Key = (unsigned char*) malloc(sizeof(secp256k1_pubkey));
    LoadCFile(filename, key->ed25Key, sizeof(secp256k1_pubkey));
    break;
  }
  default: {
    Panic("unimplemented");
  }
  }
  return key;
}

PrivKey* LoadPrivateKey(const string &filename, KeyType t, bool precompute) {
  PrivKey* key = (PrivKey*) malloc(sizeof(PrivKey));
  key->t = t;
  switch(key->t) {
  case RSA: {
    key->rsaKey = new RSA::PrivateKey();
    ByteQueue queue;
    Load(filename, queue);

    key->rsaKey->Load(queue);
    break;
  }
  case ECDSA: {
    key->ecdsaKey = new CryptoPP::ECDSA<ECP, SHA256>::PrivateKey();
    ByteQueue queue;
    Load(filename, queue);

    key->ecdsaKey->Load(queue);
    if (precompute) {
      key->ecdsaKey->Precompute();
    }
    break;
  }
  case ED25: {
    key->ed25Key = (unsigned char*) malloc(crypto_sign_SECRETKEYBYTES);
    LoadCFile(filename, key->ed25Key, crypto_sign_SECRETKEYBYTES);
    break;
  }
  case SECP: {
    key->ed25Key = (unsigned char*) malloc(32);
    LoadCFile(filename, key->ed25Key, 32);
    break;
  }
  default: {
    Panic("unimplemented");
  }
  }
  return key;
}

std::pair<PrivKey*, PubKey*> GenerateKeypair(KeyType t, bool precompute) {
  PrivKey* privKey = (PrivKey*) malloc(sizeof(PrivKey));
  privKey->t = t;
  PubKey* pubKey = (PubKey*) malloc(sizeof(PubKey));
  pubKey->t = t;

  switch(t) {
  case RSA: {
    // PGP Random Pool-like generator
    AutoSeededRandomPool prng;

    // generate keys
    privKey->rsaKey = new RSA::PrivateKey();
    privKey->rsaKey->Initialize(prng, 2048);

    pubKey->rsaKey = new RSA::PublicKey(*privKey->rsaKey);
    bool result = pubKey->rsaKey->Validate(prng, 3);
    if (!result) {
      throw "Public key derivation failed";
    }
    break;
  }
  case ECDSA: {
    // PGP Random Pool-like generator
    AutoSeededRandomPool prng;

    // generate keys
    privKey->ecdsaKey = new CryptoPP::ECDSA<ECP, SHA256>::PrivateKey();
    privKey->ecdsaKey->Initialize(prng, ASN1::secp256k1());

    pubKey->ecdsaKey = new CryptoPP::ECDSA<ECP, SHA256>::PublicKey();
    privKey->ecdsaKey->MakePublicKey(*pubKey->ecdsaKey);
    bool result = pubKey->ecdsaKey->Validate(prng, 3);
    if (!result) {
      throw "Public key derivation failed";
    }
    if (precompute) {
      privKey->ecdsaKey->Precompute();
      pubKey->ecdsaKey->Precompute();
    }
    break;
  }
  case ED25: {
    pubKey->ed25Key = (unsigned char*) malloc(crypto_sign_PUBLICKEYBYTES);
    privKey->ed25Key = (unsigned char*) malloc(crypto_sign_SECRETKEYBYTES);
    crypto_sign_keypair(pubKey->ed25Key, privKey->ed25Key);
    break;
  }
  case SECP: {
    pubKey->secpKey = (unsigned char*) malloc(sizeof(secp256k1_pubkey));
    privKey->secpKey = (unsigned char*) malloc(32);
    // TODO prolly not secure random but meh
    std::random_device rd;
    for (int i = 0; i < 32; i++) {
      privKey->secpKey[i] = static_cast<char>(rd());
    }
    if (secp256k1_ec_pubkey_create(secpCTX, (secp256k1_pubkey*) pubKey->secpKey, privKey->secpKey) == 0) {
      Panic("Unable to create pub key");
    };
    break;
  }
  default: {
    Panic("unimplemented");
  }
  }
  return std::pair<PrivKey*, PubKey*>(privKey, pubKey);
}

}  // namespace crypto
