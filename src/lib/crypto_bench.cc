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

  crypto::PrivKey privKey(crypto::GeneratePrivateKey());
  crypto::PubKey pubKey(crypto::DerivePublicKey(privKey));

  struct Latency_t signLat;
  struct Latency_t verifyLat;
  _Latency_Init(&signLat, "sign");
  _Latency_Init(&verifyLat, "verify");
  for (uint64_t i = 0; i < FLAGS_iterations; ++i) {
    std::string s;
    GenerateRandomString(FLAGS_size, rd, s);
    Latency_Start(&signLat);
    std::string sig(crypto::Sign(privKey, s));
    Latency_End(&signLat);
    Latency_Start(&verifyLat);
    crypto::Verify(pubKey, s, sig);
    Latency_End(&verifyLat);
  }
  Latency_Dump(&signLat);
  Latency_Dump(&verifyLat);
  return 0;
}
