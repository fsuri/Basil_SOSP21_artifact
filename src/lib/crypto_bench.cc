#include "lib/latency.h"
#include "lib/crypto.h"

#include <gflags/gflags.h>

#include <random>

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

  crypto::KeyType keyType = crypto::ED25;
  bool precompute = true;

  std::pair<crypto::PrivKey*, crypto::PubKey*> keypair = crypto::GenerateKeypair(keyType, precompute);
  crypto::PrivKey* privKey = keypair.first;
  crypto::PubKey* pubKey = keypair.second;

  struct Latency_t signLat;
  struct Latency_t verifyLat;
  struct Latency_t hashLat;
  _Latency_Init(&signLat, "sign");
  _Latency_Init(&verifyLat, "verify");
  _Latency_Init(&hashLat, "sha256");
  for (uint64_t i = 0; i < FLAGS_iterations; ++i) {
    std::string s;
    GenerateRandomString(FLAGS_size, rd, s);
    Latency_Start(&signLat);
    std::string sig(crypto::Sign(privKey, s));
    Latency_End(&signLat);
    Latency_Start(&verifyLat);
    crypto::Verify(pubKey, s, sig);
    Latency_End(&verifyLat);

    std::string hs;
    GenerateRandomString(FLAGS_size, rd, hs);

    Latency_Start(&hashLat);
    std::string digest = crypto::Hash(hs);
    Latency_End(&hashLat);
  }
  Latency_Dump(&signLat);
  Latency_Dump(&verifyLat);
  Latency_Dump(&hashLat);
  return 0;
}
