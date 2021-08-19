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
#ifndef _NODE_CRYPTO_H_
#define _NODE_CRYPTO_H_


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

//#include <openssl/sha.h> //this is what the donna lib uses. cant have both this and crytoppp though.
#include "lib/ed25519.h" // Donna ed25519 lib   https://github.com/justmoon/curvebench/tree/master/src/ed25519-donna

#include <string>

namespace crypto {

  using namespace CryptoPP;
  using namespace std;

enum KeyType { RSA, ECDSA, ED25, SECP, DONNA };

// typedef struct PubKey PubKey;
// typedef struct PrivKey PrivKey;

struct PubKey {
  KeyType t;
  union {
    RSA::PublicKey* rsaKey;
    CryptoPP::ECDSA<ECP, SHA256>::PublicKey* ecdsaKey;
    unsigned char* ed25Key;
    unsigned char* secpKey;
    ed25519_public_key* donnaKey;
  };
};

struct PrivKey {
  KeyType t;
  union {
    RSA::PrivateKey* rsaKey;
    CryptoPP::ECDSA<ECP, SHA256>::PrivateKey* ecdsaKey;
    unsigned char* ed25Key;
    unsigned char* secpKey;
    std::pair<ed25519_secret_key*,ed25519_public_key *> donnaKey;
    //TODO: Could probably refactor the keys to just be unsigned char* instead of unsigned char *[32]
    // ed25519_secret_key* donnaKeyPriv;
    // ed25519_public_key* donnaKeyPub;//ed25519_public_key donnaKeyPub;
  };
};

using namespace std;

string Hash(const string &message);
size_t HashSize();

string Sign(PrivKey* privateKey, const string &message);
size_t SigSize(PrivKey* privateKey);
size_t SigSize(PubKey* publicKey);

bool Verify(PubKey* publicKey, const char *message, size_t messageLen,
    const char *signature);

bool BatchVerify(KeyType t, PubKey* publicKeys[], const char *messages[], size_t messageLens[], const char *signatures[], int num, int *valid);
bool BatchVerifyS(KeyType t, PubKey* publicKeys[], string* messages[], size_t messageLens[], string* signatures[], int num, int *valid);

std::string HMAC(std::string message, std::string key);

bool verifyHMAC(std::string message, std::string mac, std::string key);

void SavePublicKey(const string &filename, PubKey* key);

void SavePrivateKey(const std::string &filename, PrivKey* key);

PubKey* LoadPublicKey(const string &filename, KeyType t, bool precompute);

PrivKey* LoadPrivateKey(const string &filename, KeyType t, bool precompute);

std::pair<PrivKey*, PubKey*> GenerateKeypair(KeyType t, bool precompute);

// TODO should have canonical serialization for this to be correct,
// but this should be fine for now
template <typename S>
void SignMessage(PrivKey* privateKey, const std::string &m, S &s) {
  string signature = Sign(privateKey, m);

  s.set_signature(signature);
}

template <typename S>
bool IsMessageValid(PubKey* publicKey, const std::string &m, S *s) {
  return Verify(publicKey, &m[0], m.length(), &s->signature()[0]);
}

}  // namespace crypto

#endif /* _NODE_CRYPTO_H_ */
