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


  //Test store and load.
  std::string keyFileName("keytest");
  const std::string privateKeyName = keyFileName + ".priv";
  crypto::SavePrivateKey(privateKeyName, privKey);
  std::cout << "Saved private key to: " << privateKeyName << std::endl;

  const std::string publicKeyName = keyFileName + ".pub";
  //(*pubKey->donnaKey)[strlen((*pubKey->donnaKey))-1] = '\0';
  crypto::SavePublicKey(publicKeyName, pubKey);
  std::cout << "Saved public key to: " << publicKeyName << std::endl;

  unsigned char* test = *(pubKey->donnaKey);
  bool b = *(pubKey->donnaKey) == test;
  std::cout << "Pub vs char conversion: " << b <<std::endl;


  // crypto::PubKey* keytest = (crypto::PubKey*) malloc(sizeof(crypto::PubKey));
  // keytest->t = crypto::DONNA;
  // keytest->donnaKey = (ed25519_public_key*) malloc(sizeof(ed25519_public_key));
  //
  // const std::string filename = publicKeyName;
  // FILE * file = fopen(filename.c_str(), "r");
  // if (file == NULL) {
  //   Panic("Invalid filename %s", filename.c_str());
  // }
  //
  // unsigned char* bytes = *(keytest->donnaKey);
  // fread(bytes, sizeof(unsigned char), sizeof(ed25519_public_key), file);
  // std::cout << "Pub vs Load conversion: " << *(pubKey->donnaKey) << " ...  " << *(keytest->donnaKey) <<std::endl;
  // bool c = *(pubKey->donnaKey) == *(keytest->donnaKey);
  // std::cout << "Pub vs Load conversion: " << c <<std::endl;

  std::cout << "Priv.pub vs Pub " << privKey->donnaKey.second << " ... " << pubKey->donnaKey << std::endl;

  crypto::PrivKey* privKeyLOAD = crypto::LoadPrivateKey(privateKeyName, crypto::DONNA, false);
  bool first = *(privKey->donnaKey.first) == *(privKeyLOAD->donnaKey.first);
  bool second = *(privKey->donnaKey.second) == *(privKeyLOAD->donnaKey.second);
  std::cout << "Original vs Load: " << *privKey->donnaKey.first << " ... " << *privKeyLOAD->donnaKey.first << std::endl;
  std::cout << "Original vs Load: " << *privKey->donnaKey.second << " ... " << *privKeyLOAD->donnaKey.second << std::endl;
  std::cout << "ASSERT LOAD: " << first << "  " << second << std::endl;

  crypto::PubKey* pubKeyLOAD = crypto::LoadPublicKey(publicKeyName, crypto::DONNA, false);
  // crypto::PubKey* keytest = (crypto::PubKey*) malloc(sizeof(crypto::PubKey));
  // keytest->t = crypto::DONNA;
  // keytest->donnaKey = (ed25519_public_key*) malloc(sizeof(ed25519_public_key));
  //
  // const std::string filename = publicKeyName;
  // FILE * file = fopen(filename.c_str(), "r");
  // if (file == NULL) {
  //   Panic("Invalid filename %s", filename.c_str());
  // }
  //
  // unsigned char* bytes = *(keytest->donnaKey);
  // fread(bytes, sizeof(unsigned char), sizeof(ed25519_public_key), file);
  // std::cout << "Pub vs Load conversion: " << *(pubKey->donnaKey) << " ...  " << *(keytest->donnaKey) <<std::endl;
  // bool c = *(pubKey->donnaKey) == *(keytest->donnaKey);
  // std::cout << "Pub vs Load conversion: " << c <<std::endl;

  // for(int i = 0; i<32; ++i){
  //   std::cout << (*pubKey->donnaKey)[i] << std::endl;
  // }

  char fpubKey[33];
  memcpy((void*)fpubKey, (void*) *pubKey->donnaKey, 32*sizeof(unsigned char));
  fpubKey[32] = '\0';

  char fpubKeyLOAD[33];
  memcpy((void*) fpubKeyLOAD, (void*)*pubKeyLOAD->donnaKey, 32*sizeof(unsigned char));
  fpubKeyLOAD[32] = '\0';
  bool c = *fpubKey == *fpubKeyLOAD;
  std::cout << "Pub vs Load conversion: " << c <<std::endl;
  std::cout << "Original vs Load: " << fpubKey << " vs " << fpubKeyLOAD <<std::endl;



  std::cout << "SIZE: " << sizeof(ed25519_public_key) << std::endl;
  //privKey = privKeyLOAD;
  ///Test end

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

    // crypto::PubKey* publicKeys[num];
    // const char *messages[num];
    // size_t messageLens[num];
    // const char *signatures[num];
    //ed25519_signature sig[num];


    //USE vectors
    //std::vector<double> v;   double* a = &v[0];  OR: v.data()
    std::vector<crypto::PubKey*> publicKeys;
    std::vector<const char*> messages;
    std::vector<size_t> messageLens;
    std::vector<const char*> signatures;

    std::string strings[num];
    std::string sign[num];
    int valid[num];

    int iter = 8;
    if(keyType != crypto::DONNA){ continue;}
    else{
      for(int i=0; i<iter; i++){
            //DIFFERENT STRINGS

         GenerateRandomString(FLAGS_size, rd, strings[i]);

            //DIFFERENT KEYS
         std::pair<crypto::PrivKey*, crypto::PubKey*> keypair = crypto::GenerateKeypair(keyType, precompute);
         crypto::PrivKey* privKeyLOAD = keypair.first;
         crypto::PubKey* pubKeyLOAD = keypair.second;

         //Test Storing and Loading
         std::string keyFileName("keytest");
         const std::string publicKeyName = keyFileName + ".pub";
         crypto::SavePublicKey(publicKeyName, pubKeyLOAD);
         crypto::PubKey* pubKey = crypto::LoadPublicKey(publicKeyName, crypto::KeyType::DONNA, false);

         const std::string privateKeyName = keyFileName + ".priv";
         crypto::SavePrivateKey(privateKeyName, privKeyLOAD);
         crypto::PrivKey* privKey = crypto::LoadPrivateKey(privateKeyName, crypto::DONNA, false);


        sign[i] = (crypto::Sign(privKey, strings[i]));


        //speed up only when the messages and keys are the same?? Experiments that modify either seemingly dont get a speedup...
        // publicKeys[i] = pubKey;
        // messages[i] = &(strings[i])[0];
        // messageLens[i] = s.length();
        // signatures[i] = &(sign[i])[0];

        publicKeys.push_back(pubKey);
        messages.push_back(&(strings[i])[0]);
        messageLens.push_back(s.length());
        signatures.push_back(&(sign[i])[0]);
      }
      Latency_Start(&batchLat);
    //  assert(crypto::BatchVerify(crypto::KeyType::DONNA, publicKeys, messages, messageLens, signatures, iter, &valid[0]));
      assert(crypto::BatchVerify(crypto::KeyType::DONNA, publicKeys.data(), messages.data(), messageLens.data(), signatures.data(), iter, &valid[0]));
      // if(i==0){
        // for(int j =0; j<iter; j++){
        //   std::cout << "ENTRY:" << valid[j] << std::endl;
        // }
      // }
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
