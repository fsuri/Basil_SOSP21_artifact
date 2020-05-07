#include "lib/assert.h"
#include "lib/batched_sigs.h"
#include <cstring>
#include <unordered_map>
#include "blake3.h"

namespace BatchedSigs {

void packInt(unsigned int i, unsigned char* out) {
  out[0] = ((unsigned int) i >> 24) & 0xFF;
  out[1] = ((unsigned int) i >> 16) & 0xFF;
  out[2] = ((unsigned int) i >> 8) & 0xFF;
  out[3] = ((unsigned int) i) & 0xFF;
}

unsigned int unpackInt(unsigned char* in) {
  unsigned int tmp = 0;
  tmp = in[0];
  tmp = (tmp << 8) | in[1];
  tmp = (tmp << 8) | in[2];
  tmp = (tmp << 8) | in[3];
  return tmp;
}

blake3_hasher hasher;
void bhash(unsigned char* in, size_t len, unsigned char* out) {
  blake3_hasher_init(&hasher);

  blake3_hasher_update(&hasher, in, len);

  // Finalize the hash. BLAKE3_OUT_LEN is the default output length, 32 bytes.
  blake3_hasher_finalize(&hasher, out, BLAKE3_OUT_LEN);
}

void bhash_cat(unsigned char* in1, unsigned char* in2, unsigned char* out) {
  blake3_hasher_init(&hasher);

  blake3_hasher_update(&hasher, in1, BLAKE3_OUT_LEN);
  blake3_hasher_update(&hasher, in2, BLAKE3_OUT_LEN);

  // Finalize the hash. BLAKE3_OUT_LEN is the default output length, 32 bytes.
  blake3_hasher_finalize(&hasher, out, BLAKE3_OUT_LEN);
}

#include <stdint.h>
static inline uint32_t log2(const uint32_t x) {
  uint32_t y;
  asm ( "\tbsr %1, %0\n"
      : "=r"(y)
      : "r" (x)
  );
  return y;
}

std::vector<std::string> generateBatchedSignatures(std::vector<std::string> messages, crypto::PrivKey* privateKey) {
  // generate the merkel tree
  unsigned int n = messages.size();
  assert(n > 0);
  size_t hash_size = BLAKE3_OUT_LEN;

  unsigned char* tree = (unsigned char*) malloc(hash_size*(2*n - 1));
  for (unsigned int i = 0; i < n; i++) {
    bhash((unsigned char*) &messages[i][0], messages[i].length(), &tree[(n - 1 + i)*hash_size]);
  }

  for (int i = 2*n - 2; i >= 2; i-=2) {
    bhash_cat(&tree[(i-1)*hash_size], &tree[(i)*hash_size], &tree[((i/2) - 1)*hash_size]);
  }

  std::string rootHash(&tree[0], &tree[hash_size]);
  std::string rootSig = crypto::Sign(privateKey, rootHash);

  std::vector<std::string> sigs;
  size_t sig_size = crypto::SigSize(privateKey);

  size_t max_size = sig_size + 4 + 4 + (log2(n) + 1)*hash_size;
  unsigned char* sig = (unsigned char*) malloc(max_size);
  memcpy(&sig[0], &rootSig[0], sig_size);
  packInt(n, &sig[sig_size]);
  unsigned int starting_pos = sig_size + 8;

  for (unsigned int i = 0; i < n; i++) {
    packInt(i, &sig[sig_size+4]);
    int h = 0;
    for (int j = n - 1 + i; j >= 1; j=(j+1)/2 - 1) {
      memcpy(&sig[starting_pos + h*hash_size], &tree[(j % 2 == 0 ? j - 1 : j + 1)*hash_size], hash_size);
      h++;
    }
    std::string sigstr(&sig[0], &sig[starting_pos + h*hash_size]);
    sigs.push_back(sigstr);
  }

  return sigs;
}

// cache mapping root sig to hash that it verifies
std::unordered_map<std::string, std::string> cache;

bool verifyBatchedSignature(std::string signature, std::string message, crypto::PubKey* publicKey) {
  size_t hash_size = BLAKE3_OUT_LEN;
  size_t sig_size = crypto::SigSize(publicKey);

  std::string rootSig = signature.substr(0, sig_size);
  unsigned int n = unpackInt((unsigned char*) &signature[sig_size]);
  unsigned int i = unpackInt((unsigned char*) &signature[sig_size+4]);
  unsigned int starting_pos = sig_size + 8;

  unsigned char hash[BLAKE3_OUT_LEN];
  unsigned char hashout[BLAKE3_OUT_LEN];
  bhash((unsigned char*) &message[0], message.length(), &hash[0]);
  int h = 0;
  for (int j = n - 1 + i; j >= 1; j=(j+1)/2 - 1) {
    if (j % 2 == 0) {
      bhash_cat((unsigned char*) &signature[starting_pos + h*hash_size], &hash[0], hashout);
    } else {
      bhash_cat(&hash[0], (unsigned char*) &signature[starting_pos + h*hash_size], hashout);
    }
    memcpy(&hash[0], &hashout[0], hash_size);
    h++;
  }
  std::string hashStr(&hash[0], &hash[hash_size]);

  if (cache.find(rootSig) != cache.end()) {
    return cache[rootSig] == hashStr;
  } else {
    if (crypto::Verify(publicKey, hashStr, rootSig)) {
      cache[rootSig] = hashStr;
      return true;
    } else {
      return false;
    }
  }
}

}
