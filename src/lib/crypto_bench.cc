#include "lib/latency.h"
#include "lib/crypto.h"
#include "lib/batched_sigs.h"
#include "lib/blake3.h"

#include <gflags/gflags.h>

#include <random>

DEFINE_uint64(size, 1000, "size of data to verify.");
DEFINE_uint64(iterations, 100, "number of iterations to measure.");

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

  crypto::KeyType keyType = crypto::SECP;
  bool precompute = true;

  std::pair<crypto::PrivKey*, crypto::PubKey*> keypair = crypto::GenerateKeypair(keyType, precompute);
  crypto::PrivKey* privKey = keypair.first;
  crypto::PubKey* pubKey = keypair.second;

  struct Latency_t signLat;
  struct Latency_t verifyLat;
  struct Latency_t signBLat;
  struct Latency_t verifyBLat;
  struct Latency_t hashLat;
  struct Latency_t blake3Lat;
  _Latency_Init(&signLat, "sign");
  _Latency_Init(&verifyLat, "verify");
  _Latency_Init(&signBLat, "sign");
  _Latency_Init(&verifyBLat, "verify");
  _Latency_Init(&hashLat, "sha256");
  _Latency_Init(&blake3Lat, "blake3");
  for (uint64_t i = 0; i < FLAGS_iterations; ++i) {
    std::string s;
    GenerateRandomString(FLAGS_size, rd, s);
    Latency_Start(&signLat);
    std::string sig(crypto::Sign(privKey, s));
    Latency_End(&signLat);
    Latency_Start(&verifyLat);
    assert(crypto::Verify(pubKey, s, sig));
    Latency_End(&verifyLat);

    std::string hs;
    GenerateRandomString(FLAGS_size, rd, hs);

    Latency_Start(&hashLat);
    std::string digest = crypto::Hash(hs);
    Latency_End(&hashLat);


    std::string hs2;
    GenerateRandomString(FLAGS_size, rd, hs2);

    Latency_Start(&blake3Lat);
      // Initialize the hasher.
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);

    // Read input bytes from stdin.
    blake3_hasher_update(&hasher, &hs2[0], hs2.length());

    // Finalize the hash. BLAKE3_OUT_LEN is the default output length, 32 bytes.
    uint8_t output[BLAKE3_OUT_LEN];
    blake3_hasher_finalize(&hasher, output, BLAKE3_OUT_LEN);
    Latency_End(&blake3Lat);

    std::vector<std::string> messages;
    int nmsgs = 4;
    for (int i = 0; i < nmsgs; i++) {
      std::string tmp;
      GenerateRandomString(FLAGS_size, rd, tmp);
      messages.push_back(tmp);
    }
    Latency_Start(&signBLat);
    std::vector<std::string> sigs = BatchedSigs::generateBatchedSignatures(messages, privKey);
    Latency_End(&signBLat);
    Latency_Start(&verifyBLat);
    for (int i = 0; i < nmsgs; i++) {
      assert(BatchedSigs::verifyBatchedSignature(sigs[i], messages[i], pubKey));
    }
    Latency_End(&verifyBLat);

  }
  Latency_Dump(&signLat);
  Latency_Dump(&verifyLat);
  Latency_Dump(&signBLat);
  Latency_Dump(&verifyBLat);
  Latency_Dump(&hashLat);
  Latency_Dump(&blake3Lat);
  return 0;
}
