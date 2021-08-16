/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fs435@cornell.edu>
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
#include "lib/crypto.h"
#include "lib/assert.h"

#include <string.h>

//add batching interface too. !!

//TODO (FS): Add BLS    https://github.com/skalenetwork/libBLS

namespace crypto {

using namespace CryptoPP;
using namespace std;

thread_local secp256k1_context *secpCTX;
// hasher struct
thread_local blake3_hasher hasher;

static_block {
  secpCTX = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
}

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

//FS Use a different Hash? I.e. Blake for everything?
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
  case DONNA: {
    //REQUIRES PUBLIC KEY TO SIGN AS WELL
    //ed25519_signature sig;
    std::string signature;
    signature.resize(64); //???? this creates leaks? but without it it crashes.

    const unsigned char * msg = (unsigned char*) &message[0];

    ed25519_sign(msg, message.length(), *privateKey->donnaKey.first,
    *privateKey->donnaKey.second, (unsigned char*) &signature[0]);
    //ed25519_sign(msg, sizeof(msg)-1, *privateKey->donnaKey.first, *privateKey->donnaKey.second, sig);
    // signature = std::string( (const char*)  sig);

    // bool one = ed25519_sign_open(msg, message.length(), *privateKey->donnaKey.second, (unsigned char*) &signature[0]) == 0;
    // std::cout << "SIGNING RESULT:" << one << std::endl;

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
  case DONNA:{
    return sizeof(ed25519_signature);
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


bool Verify(PubKey* publicKey, const char *message, size_t messageLen,
    const char *signature) {
  switch(publicKey->t) {
    case RSA: {
      Panic("Not implemented.");
      /*bool result = false;
      RSASS<PSS, SHA256>::Verifier verifier(*publicKey->rsaKey);
      StringSource ss2(
          signature + message, true,
          new SignatureVerificationFilter(
              verifier, new ArraySink((uint8_t *)&result, sizeof(result))));
      return result;*/
    }
    case ECDSA: {
      Panic("Not implemented.");
      /*bool result = false;
      CryptoPP::ECDSA<ECP, SHA256>::Verifier verifier(*publicKey->ecdsaKey);
      StringSource ss2(
          signature + message, true,
          new SignatureVerificationFilter(
              verifier, new ArraySink((uint8_t *)&result, sizeof(result))));
      return result;*/
    }
    case ED25: {
      return crypto_sign_verify_detached((unsigned char*) signature,
          (unsigned char*) message, messageLen, publicKey->ed25Key) == 0;
    }
    case SECP: {
      blake3_hasher_init(&hasher);
      blake3_hasher_update(&hasher, (unsigned char*) message, messageLen);
      unsigned char out[BLAKE3_OUT_LEN];
      blake3_hasher_finalize(&hasher, out, BLAKE3_OUT_LEN);
      return secp256k1_ecdsa_verify(secpCTX, (secp256k1_ecdsa_signature*) signature, out, (secp256k1_pubkey*) publicKey->secpKey) == 1;
    }
    case DONNA: {

      unsigned char * testMsg = (unsigned char*) &message[0];
      //int len = static_cast<int>(messageLen);
      bool res = ed25519_sign_open(testMsg, messageLen, *publicKey->donnaKey, (unsigned char*) signature) == 0;
      //std::cout << "VERIFY TEST:" << res << std::endl;

      return res;
    }

    default: {
      Panic("unimplemented");
    }
  }
}

//split into modular function: AddToBatch, VerifyBatch

bool BatchVerify(KeyType t, PubKey* publicKeys[], const char *messages[], size_t messageLens[], const char *signatures[], int num, int *valid){
  switch(t){
    case DONNA: {
      const unsigned char *pkp[num]; // = {pk, pk, pk, pk, pk, pk};

      //int valid[num];
      for(int i=0; i<num; i++){
          pkp[i] = (unsigned char*) publicKeys[i]->donnaKey;
      }


     bool all_valid = ed25519_sign_open_batch((const unsigned char**) messages, messageLens, pkp, (const unsigned char**) signatures, num, valid) == 0;
     return all_valid;
    }
   default: {
     Panic("Batch Verification only available for ed25519_DONNA based cryptography");
   }
  }
}

bool BatchVerifyS(KeyType t, PubKey* publicKeys[], string* messages[], size_t messageLens[], string* signatures[], int num, int *valid){
  switch(t){
    case DONNA: {
      const unsigned char *pkp[num]; // = {pk, pk, pk, pk, pk, pk};
      const unsigned char *mkp[num];
      const unsigned char *skp[num];
      //int valid[num];

      for(int i=0; i<num; i++){
          pkp[i] = (const unsigned char*) publicKeys[i]->donnaKey;
          mkp[i] = (const unsigned char*) messages[i]->data(); //&(*messages[i])[0]; // .c_str();
          skp[i] = (const unsigned char*) signatures[i]->data(); //&(*signatures[i])[0];   //.c_str();
      }


     bool all_valid = ed25519_sign_open_batch(mkp, messageLens, pkp, skp, num, valid) == 0;
     return all_valid;
    }
   default: {
     Panic("Batch Verification only available for ed25519_DONNA based cryptography");
   }
  }
}

//need detailed info if to be used for n

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
  case DONNA: {
    //SaveCFile(filename, (unsigned char*) &key->donnaKey[0], sizeof(ed25519_public_key));
    SaveCFile(filename, *key->donnaKey, sizeof(ed25519_public_key));
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
  case DONNA: {
    //SaveCFile(filename, (unsigned char*) &key->donnaKey.first[0], sizeof(ed25519_public_key));
    SaveCFile(filename, *key->donnaKey.first, sizeof(ed25519_public_key));
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
  case DONNA: {
    key->donnaKey = (ed25519_public_key*) malloc(sizeof(ed25519_public_key));
    //LoadCFile(filename, (unsigned char*) &key->donnaKey[0], sizeof(ed25519_public_key));  //FS: This should work?
    //TODO: cast back the other way?
    LoadCFile(filename, *(key->donnaKey), sizeof(ed25519_public_key));




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
  case DONNA: {
    string delimiter = ".priv";
    const string publicfile = filename.substr(0, filename.find(delimiter)) + ".pub"; // get publickey path name.
    key->donnaKey.first = (ed25519_secret_key*) malloc(sizeof(ed25519_secret_key));
    //LoadCFile(filename, (unsigned char*) &key->donnaKey.first[0], sizeof(ed25519_secret_key));  //FS: This should work?
    LoadCFile(filename, *key->donnaKey.first, sizeof(ed25519_secret_key));
    key->donnaKey.second = (ed25519_public_key*) malloc(sizeof(ed25519_public_key));
  //LoadCFile(publicfile, (unsigned char*) &key->donnaKey.second[0], sizeof(ed25519_public_key));  //FS: This should work?
    LoadCFile(publicfile, *key->donnaKey.second, sizeof(ed25519_public_key));

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
      case DONNA: {

        privKey->donnaKey.first = (ed25519_secret_key*) malloc(sizeof(ed25519_secret_key));
        privKey->donnaKey.second = (ed25519_public_key*) malloc(sizeof(ed25519_public_key));
        pubKey->donnaKey = (ed25519_public_key*) malloc(sizeof(ed25519_public_key));

      	ed25519_randombytes_unsafe(privKey->donnaKey.first, sizeof(ed25519_secret_key));
      	ed25519_publickey(*privKey->donnaKey.first, *privKey->donnaKey.second);
        pubKey->donnaKey = privKey->donnaKey.second;
        //memcpy(pubKey->donnaKey, privKey->donnaKey.second, sizeof(ed25519_public_key));


        //TEST HERE:
        // const char *msg2 = "hello";
    	  // unsigned char * msg = (unsigned char*) &msg2[0];
        //
        //
        // std::cout << "ORIG:" << *privKey->donnaKey.second << std::endl;
        // std::cout<< "COPY:" << *pubKey->donnaKey  << std::endl;
        //
        // std::string signature;
        //
        // ed25519_sign(msg, sizeof(msg)-1, *privKey->donnaKey.first, *privKey->donnaKey.second, (unsigned char*) &signature[0]);
        //
        //
        // int two = ed25519_sign_open(msg, sizeof(msg)-1, *privKey->donnaKey.second, (unsigned char*) &signature[0]) ;
        //
        // std::cout << "GENERATION TEST:" << two << std::endl;


        break;
      }
      default: {
        Panic("unimplemented");
      }
  }
  return std::pair<PrivKey*, PubKey*>(privKey, pubKey);
}

}  // namespace crypto
