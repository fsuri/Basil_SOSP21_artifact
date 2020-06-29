#ifndef _NODE_CRYPTO_H_
#define _NODE_CRYPTO_H_

#include <string>

namespace crypto {

enum KeyType { RSA, ECDSA, ED25, SECP, DONNA };

typedef struct PubKey PubKey;
typedef struct PrivKey PrivKey;

using namespace std;

string Hash(const string &message);
size_t HashSize();

string Sign(PrivKey* privateKey, const string &message);
size_t SigSize(PrivKey* privateKey);
size_t SigSize(PubKey* publicKey);

bool Verify(PubKey* publicKey, const char *message, size_t messageLen,
    const char *signature);

bool BatchVerify(KeyType t, PubKey* publicKeys[], const char *messages[], size_t messageLens[], const char *signatures[], int num, int *valid);

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
