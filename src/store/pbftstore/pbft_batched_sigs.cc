/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fsp@cs.cornell.edu>
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
#include "lib/assert.h"
#include "store/pbftstore/pbft_batched_sigs.h"
#include <cstring>
#include <unordered_map>
#include "lib/blake3.h"
#include <stdint.h>

namespace pbftBatchedSigs {

// store an int into an unsigned char array
void packInt(unsigned int i, unsigned char* out) {
  out[0] = ((unsigned int) i >> 24) & 0xFF;
  out[1] = ((unsigned int) i >> 16) & 0xFF;
  out[2] = ((unsigned int) i >> 8) & 0xFF;
  out[3] = ((unsigned int) i) & 0xFF;
}

// interpret 4 bytes from an unsigned char array as an int
unsigned int unpackInt(unsigned char* in) {
  unsigned int tmp = 0;
  tmp = in[0];
  tmp = (tmp << 8) | in[1];
  tmp = (tmp << 8) | in[2];
  tmp = (tmp << 8) | in[3];
  return tmp;
}

// hasher struct
blake3_hasher hasher;

// hash [len] bytes from [in] into [out]. Requires that
// [out] is BLAKE3_OUT_LEN bytes. It is safe for [out] to alias [in]
void bhash(unsigned char* in, size_t len, unsigned char* out) {
  // need to initialize on every hash
  blake3_hasher_init(&hasher);

  // hash the input array
  blake3_hasher_update(&hasher, in, len);

  // copy the digest into the output array
  blake3_hasher_finalize(&hasher, out, BLAKE3_OUT_LEN);
}

// hash BLAKE3_OUT_LEN bytes from [in1] with [in2] (not commutavie) into [out].
// Requires that [out] is BLAKE3_OUT_LEN bytes. It is safe for [out] to alias [in1] or [in2]
void bhash_cat(unsigned char* in1, unsigned char* in2, unsigned char* out) {
  blake3_hasher_init(&hasher);

  blake3_hasher_update(&hasher, in1, BLAKE3_OUT_LEN);
  blake3_hasher_update(&hasher, in2, BLAKE3_OUT_LEN);

  // Finalize the hash. BLAKE3_OUT_LEN is the default output length, 32 bytes.
  blake3_hasher_finalize(&hasher, out, BLAKE3_OUT_LEN);
}

// compute the log2 of [x] with an efficient assembly instruction
static inline uint32_t log2(const uint32_t x) {
  uint32_t y;
  asm ( "\tbsr %1, %0\n"
      : "=r"(y)
      : "r" (x)
  );
  return y;
}

// generate batches signatures for every message in [messages] using [privateKey]
void generateBatchedSignatures(std::vector<std::string*> messages, crypto::PrivKey* privateKey, std::vector<std::string*> sigs) {
  unsigned int n = messages.size();
  assert(n > 0);
  size_t hash_size = BLAKE3_OUT_LEN;

  // allocate the merkle tree in heap form (i.left = 2i, i.right = 2i+1)
  unsigned char* tree = (unsigned char*) malloc(hash_size*(2*n - 1));
  // insert the message hashes into the tree
  for (unsigned int i = 0; i < n; i++) {
    bhash((unsigned char*) &messages[i]->at(0), messages[i]->length(), &tree[(n - 1 + i)*hash_size]);
  }

  // compute the hashes going up the tree
  for (int i = 2*n - 2; i >= 2; i-=2) {
    bhash_cat(&tree[(i-1)*hash_size], &tree[(i)*hash_size], &tree[((i/2) - 1)*hash_size]);
  }

  // sign the hash at the root of the tree
  std::string rootHash(&tree[0], &tree[hash_size]);
  std::string rootSig = crypto::Sign(privateKey, rootHash);

  size_t sig_size = crypto::SigSize(privateKey);

  // figure out the maximum size of a signature
  size_t max_size = sig_size + 4 + 4 + (log2(n) + 1)*hash_size;
  unsigned char* sig = (unsigned char*) malloc(max_size);
  // put the root signature and [n] into every signature
  memcpy(&sig[0], &rootSig[0], sig_size);
  packInt(n, &sig[sig_size]);
  // compute the position to start placing hashes in the signature
  unsigned int starting_pos = sig_size + 8;

  for (unsigned int i = 0; i < n; i++) {
    // add the message's index to the signature
    packInt(i, &sig[sig_size+4]);
    // h is the number of hashes already appended to the signature
    int h = 0;
    // j represents the current node we are at in the tree (j+1/2 - 1 gets us to the parent)
    // we want to append j's sibling node to the signature because we assume that
    // we already have enough information to compute the hash of node j at this point
    for (int j = n - 1 + i; j >= 1; j=(j+1)/2 - 1) {
      // append the next hash on the path to the root to the signature
      memcpy(&sig[starting_pos + h*hash_size], &tree[(j % 2 == 0 ? j - 1 : j + 1)*hash_size], hash_size);
      h++;
    }
    // replace the sig with the raw signature bytes (performs a copy)
    sigs[i]->replace(0, starting_pos + h*hash_size, reinterpret_cast<const char*>(&sig[0]), starting_pos + h*hash_size);
    assert(sigs[i]->size() == starting_pos + h*hash_size);
  }
}

// cache mapping root sig to hash that it verifies
// map from signatures to hashes because it is _possible_ for two nodes
// to have the same batch and thus the same merkle tree
std::unordered_map<std::string, std::string> batchedSigsCache;

bool verifyBatchedSignature(const std::string* signature, const std::string* message, crypto::PubKey* publicKey) {
  size_t hash_size = BLAKE3_OUT_LEN;
  size_t sig_size = crypto::SigSize(publicKey);

  // get the signature of the root, the number of signatures in the batch (n)
  // and this message's index in the batch (i)
  std::string rootSig = signature->substr(0, sig_size);
  unsigned int n = unpackInt((unsigned char*) &signature->at(sig_size));
  unsigned int i = unpackInt((unsigned char*) &signature->at(sig_size+4));
  // compute the position where the hashes start in the signature
  unsigned int starting_pos = sig_size + 8;

  // this will store the hash as we traverse the merkle tree
  unsigned char hash[BLAKE3_OUT_LEN];
  // the leaf hash is the hash of the message
  bhash((unsigned char*) &message->at(0), message->length(), &hash[0]);
  // h is the index of the sibling hash in the signature
  int h = 0;
  // j is the current position in the tree.
  // invariant: [hash] is the hash of node j in the merkle tree
  for (int j = n - 1 + i; j >= 1; j=(j+1)/2 - 1) {
    if (j % 2 == 0) {
      // node j was the right sibling
      bhash_cat((unsigned char*) &signature->at(starting_pos + h*hash_size), &hash[0], hash);
    } else {
      // node j was the left sibling
      bhash_cat(&hash[0], (unsigned char*) &signature->at(starting_pos + h*hash_size), hash);
    }
    h++;
  }
  std::string hashStr(&hash[0], &hash[hash_size]);

  if (batchedSigsCache.find(rootSig) != batchedSigsCache.end()) {
    // if the root signature exists in the cache, check if it verifies the hash
    // derived from the signature
    return batchedSigsCache[rootSig] == hashStr;
  } else {
    // otherwise, verify the computed root hash
    if (crypto::Verify(publicKey, &hashStr[0], hashStr.length(), &rootSig[0])) {
      // if it is valid, store it in the cache.
      batchedSigsCache[rootSig] = hashStr;
      return true;
    } else {
      return false;
    }
  }
}

}
