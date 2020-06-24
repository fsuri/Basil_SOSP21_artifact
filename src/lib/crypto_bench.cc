#include "lib/latency.h"
#include "lib/crypto.h"
#include "lib/batched_sigs.h"
#include "lib/blake3.h"
#include "lib/message.h"

#include <gflags/gflags.h>

#include <random>

DEFINE_uint64(size, 1000, "size of data to verify.");
DEFINE_uint64(iterations, 100, "number of iterations to measure.");
DEFINE_string(signature_alg, "ecdsa", "algorithm to benchmark (options: ecdsa, ed25519, rsa, secp256k1)");

void GenerateRandomString(uint64_t size, std::random_device &rd, std::string &s) {
  s.clear();
  for (uint64_t i = 0; i < size; ++i) {
    s.push_back(static_cast<char>(rd()));
  }
}

void GenerateRandomString(uint64_t size, std::random_device &rd, std::string* s) {
  s->clear();
  for (uint64_t i = 0; i < size; ++i) {
    s->push_back(static_cast<char>(rd()));
  }
}

int main(int argc, char *argv[]) {
  gflags::SetUsageMessage("benchmark signature verification.");
	gflags::ParseCommandLineFlags(&argc, &argv, true);

  std::random_device rd;

  crypto::KeyType keyType;
  if (FLAGS_signature_alg == "ecdsa") {
    keyType = crypto::ECDSA;
  } else if (FLAGS_signature_alg == "rsa") {
    keyType = crypto::RSA;
  } else if (FLAGS_signature_alg == "ed25519") {
    keyType = crypto::ED25;
  } else if (FLAGS_signature_alg == "secp256k1") {
    keyType = crypto::SECP;
  } else if (FLAGS_signature_alg == "donna"){
    keyType = crypto::DONNA;
  } else {
    Panic("Unknown signature algorithm: %s.", FLAGS_signature_alg.c_str());
  }

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

  struct Latency_t batchLat;
  _Latency_Init(&signLat, "sign");
  _Latency_Init(&verifyLat, "verify");
  _Latency_Init(&signBLat, "sign");
  _Latency_Init(&verifyBLat, "verify");
  _Latency_Init(&hashLat, "sha256");
  _Latency_Init(&blake3Lat, "blake3");
  _Latency_Init(&batchLat, "batchverify");

  std::vector<std::string*> messages;
  int batchSize = 100;
  for (int i = 0; i < batchSize; i++) {
    messages.push_back(new std::string());
  }
  std::vector<std::string*> sigs;
  for (int i = 0; i < batchSize; i++) {
    sigs.push_back(new std::string());
  }

  Notice("===================================");
  Notice("Running %s for %lu iterations with data size %lu.", FLAGS_signature_alg.c_str(),
      FLAGS_iterations, FLAGS_size);
  for (uint64_t i = 0; i < FLAGS_iterations; ++i) {
    std::string s;
    GenerateRandomString(FLAGS_size, rd, s);
    Latency_Start(&signLat);
    std::string sig(crypto::Sign(privKey, s));
    Latency_End(&signLat);
    Latency_Start(&verifyLat);
    assert(crypto::Verify(pubKey, &s[0], s.length(), &sig[0]));
    Latency_End(&verifyLat);


      int num =64 ;
    crypto::PubKey* publicKeys[num];
    const char *messages[num];
    size_t messageLens[num];
    const char *signatures[num];
    //ed25519_signature sig[num];
    std::string strings[num];
    std::string sign[num];

    if(keyType != crypto::DONNA){ continue;}
    else{
      for(int i=0; i<num; i++){
            //DIFFERENT STRINGS

         GenerateRandomString(FLAGS_size, rd, strings[i]);

            //DIFFERENT KEYS
         std::pair<crypto::PrivKey*, crypto::PubKey*> keypair = crypto::GenerateKeypair(keyType, precompute);
         crypto::PrivKey* privKey = keypair.first;
         crypto::PubKey* pubKey = keypair.second;

        sign[i] = (crypto::Sign(privKey, strings[i]));


        //speed up only when the messages and keys are the same?? Experiments that modify either seemingly dont get a speedup...
        publicKeys[i] = pubKey;
        messages[i] = &(strings[i])[0];
        messageLens[i] = s.length();
        signatures[i] = &(sign[i])[0];
      }
      Latency_Start(&batchLat);
      assert(crypto::BatchVerify(crypto::KeyType::DONNA, publicKeys, messages, messageLens, signatures, num));
      Latency_End(&batchLat);
    }



    /*std::string hs;
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

    for (int i = 0; i < batchSize; i++) {
      std::string tmp;
      GenerateRandomString(FLAGS_size, rd, messages[i]);
    }
    Latency_Start(&signBLat);
    BatchedSigs::generateBatchedSignatures(messages, privKey, sigs);
    Latency_End(&signBLat);
    Latency_Start(&verifyBLat);
    for (int i = 0; i < batchSize; i++) {
      assert(BatchedSigs::verifyBatchedSignature(sigs[i], messages[i], pubKey));
    }
    Latency_End(&verifyBLat);*/

  }

  //TODO (FS): Add test case for ed25119 donna, + using batch verification.

  Latency_Dump(&signLat);
  Latency_Dump(&verifyLat);
  if(keyType == crypto::DONNA) Latency_Dump(&batchLat);
  Notice("===================================");
  //Latency_Dump(&signBLat);
  //Latency_Dump(&verifyBLat);
  //Latency_Dump(&hashLat);
  //Latency_Dump(&blake3Lat);
  return 0;
}
