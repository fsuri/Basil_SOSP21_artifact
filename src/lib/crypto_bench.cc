#include "lib/latency.h"
#include "lib/crypto.h"

#include <gflags/gflags.h>

#include <random>
#include <sodium.h>

DEFINE_uint64(size, 100, "size of data to verify.");
DEFINE_uint64(iterations, 1000, "number of iterations to measure.");

void GenerateRandomString(uint64_t size, std::random_device &rd, std::string &s) {
  s.clear();
  for (uint64_t i = 0; i < size; ++i) {
    s.push_back(static_cast<char>(rd()));
  }
}

int main(int argc, char *argv[]) {
  gflags::SetUsageMessage("benchmark signature verification.");
	gflags::ParseCommandLineFlags(&argc, &argv, true);

  std::random_device rd;


  #ifdef USE_ED25519_SIGS
  std::pair<crypto::PrivKey, crypto::PubKey> keypair = crypto::GenerateKeypair();
  crypto::PrivKey privKey = keypair.first;
  crypto::PubKey pubKey = keypair.second;
  #else
  crypto::PrivKey privKey(crypto::GeneratePrivateKey());
  crypto::PubKey pubKey(crypto::DerivePublicKey(privKey));
  #endif
  // privKey.Precompute();
  // pubKey.Precompute();

  unsigned char pk[crypto_sign_PUBLICKEYBYTES];
  unsigned char sk[crypto_sign_SECRETKEYBYTES];
  crypto_sign_keypair(pk, sk);

  {
    std::string s;
    GenerateRandomString(FLAGS_size, rd, s);
    unsigned char edsig[crypto_sign_BYTES];
    int result = crypto_sign_detached(edsig, NULL, (const unsigned char*) s.c_str(), s.length(), sk);
    std::string signature(reinterpret_cast<char*>(edsig), crypto_sign_BYTES);
    printf("Result: %d\n", result);
    result = crypto_sign_verify_detached((const unsigned char*) signature.c_str(), (const unsigned char*) s.c_str(), s.length(), pk);
    printf("Result: %d\n", result);
    std::cout << "eq " << ((bool) (result == 0)) << std::endl;
  }

  struct Latency_t signLat;
  struct Latency_t verifyLat;
  struct Latency_t hashLat;
  struct Latency_t signEDLat;
  struct Latency_t verifyEDLat;
  _Latency_Init(&signLat, "sign");
  _Latency_Init(&verifyLat, "verify");
  _Latency_Init(&hashLat, "sha256");
  _Latency_Init(&signEDLat, "sign");
  _Latency_Init(&verifyEDLat, "verify");
  for (uint64_t i = 0; i < FLAGS_iterations; ++i) {
    std::string s;
    GenerateRandomString(FLAGS_size, rd, s);
    Latency_Start(&signLat);
    std::string sig(crypto::Sign(privKey, s));
    Latency_End(&signLat);
    Latency_Start(&verifyLat);
    crypto::Verify(pubKey, s, sig);
    Latency_End(&verifyLat);

    CryptoPP::SHA256 hash;
    std::string digest;
    std::string hs;
    GenerateRandomString(FLAGS_size, rd, s);

    Latency_Start(&hashLat);
    hash.Update((const byte*) &hs[0], hs.length());
    digest.resize(hash.DigestSize());
    hash.Final((byte*) &digest[0]);
    Latency_End(&hashLat);


    unsigned char edsig[crypto_sign_BYTES];
    Latency_Start(&signEDLat);
    crypto_sign_detached(edsig, NULL, (const byte*) &s[0], s.length(), sk);
    Latency_End(&signEDLat);
    Latency_Start(&verifyEDLat);
    crypto_sign_verify_detached(edsig, (const byte*) &s[0], s.length(), pk);
    Latency_End(&verifyEDLat);
  }
  Latency_Dump(&signLat);
  Latency_Dump(&verifyLat);
  Latency_Dump(&hashLat);
  Latency_Dump(&signEDLat);
  Latency_Dump(&verifyEDLat);
  return 0;
}
