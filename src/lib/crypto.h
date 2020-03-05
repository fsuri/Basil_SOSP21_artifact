#ifndef _NODE_CRYPTO_H_
#define _NODE_CRYPTO_H_

#include <string>

#include <cryptopp/eccrypto.h>
#include <cryptopp/files.h>
#include <cryptopp/hex.h>
#include <cryptopp/oids.h>
#include <cryptopp/osrng.h>
#include <cryptopp/sha.h>

namespace crypto {

using namespace CryptoPP;
using namespace std;

using PrivKey = ECDSA<ECP, SHA256>::PrivateKey;
using PubKey = ECDSA<ECP, SHA256>::PublicKey;

string Hash(const string &message);

string Sign(const PrivKey &privateKey, const string &message);

bool Verify(const PubKey &publicKey, const string &message, string &signature);

void SavePublicKey(const string &filename, PubKey &key);

void SavePrivateKey(const std::string &filename, PrivKey &key);

PubKey LoadPublicKey(const string &filename);

PrivKey LoadPrivateKey(const string &filename);

PrivKey GeneratePrivateKey();

PubKey DerivePublicKey(PrivKey &privateKey);

// TODO should have canonical serialization for this to be correct,
// but this should be fine for now
template <typename M, typename S>
void SignMessage(const PrivKey privateKey, const M *m, S &s) {
  string serialized = m->SerializeAsString();
  string signature = Sign(privateKey, serialized);

  s.set_signature(signature);
}

template <typename S>
bool IsMessageValid(const PubKey publicKey, const std::string &m, S *s) {
  string signature = s->signature();

  return Verify(publicKey, m, signature);
}

}  // namespace crypto

#endif /* _NODE_CRYPTO_H_ */
