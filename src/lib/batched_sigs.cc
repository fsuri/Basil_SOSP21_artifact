#include "lib/assert.h"
#include "lib/batched_sigs.h"
#include <cstring>
#include <unordered_map>
#include "lib/blake3.h"
#include <stdint.h>
#include <iostream>

namespace BatchedSigs {

uint64_t hashCount = 0;
uint64_t hashCatCount = 0;

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

std::vector<uint64_t> treeHeights;
uint64_t getTreeHeight(uint64_t n, uint64_t m) {
  treeHeights.resize(n + 1, 0UL);
  if (treeHeights[n] == 0UL) {
    uint64_t k = n;
    while (k > 0) {
      k /= m;
      //std::cerr << k << std::endl;
      treeHeights[n]++;
    }
  }
  return treeHeights[n];
}
// generate batches signatures for every message in [messages] using [privateKey]
void generateBatchedSignatures(const std::vector<const std::string*> &messages, crypto::PrivKey* privateKey, std::vector<std::string> &sigs, uint64_t m) {
  unsigned int n = messages.size();
  assert(n > 0);
  size_t hash_size = BLAKE3_OUT_LEN;

  // allocate the merkle tree in heap form (i.left = 2i, i.right = 2i+1)
  uint64_t num_nodes = (m * (n - 1) + (m - 2)) / (m - 1) + 1; // add (m-2) to ensure ceil
  //std::cerr << "num nodes " << num_nodes << std::endl;
  unsigned char* tree = (unsigned char*) malloc(hash_size*num_nodes);
  // insert the message hashes into the tree
  for (unsigned int i = 0; i < n; i++) {
    //std::cerr << "placing msg " << i << " at idx "
    //          << ((n - 1 + (m - 2)) / (m - 1) + i) << std::endl;
    

    bhash((unsigned char*) &messages[i]->at(0), messages[i]->length(),
        &tree[((n - 1 + (m - 2)) / (m - 1) + i) * hash_size]);
  }
  int max_leaf = ((n - 1 + (m - 2)) / (m - 1) + n - 1);
  // compute the hashes going up the tree
  for (int i = max_leaf; i > 1; ) {
    int l = (i-1)/m * m + 1;
    //std::cerr << "node " << i << " hashing " << l << " to " << i
    //          << " for parent " << (i - 1) / m << std::endl;
    blake3_hasher_init(&hasher);
    /*for (int j = l; j <= i; ++j) {
      //std::cerr << "update with hash " << j << std::endl;
      blake3_hasher_update(&hasher, &tree[j * hash_size], BLAKE3_OUT_LEN);
    }*/
    blake3_hasher_update(&hasher, &tree[l * hash_size], (i + 1 - l) * BLAKE3_OUT_LEN);

    blake3_hasher_finalize(&hasher, &tree[(i - 1) / m * hash_size], BLAKE3_OUT_LEN);

    i = i - (i - l) - 1;
  }

  // sign the hash at the root of the tree
  std::string rootHash(&tree[0], &tree[hash_size]);
  std::string rootSig = crypto::Sign(privateKey, rootHash);

  size_t sig_size = crypto::SigSize(privateKey);

  // figure out the maximum size of a signature
  size_t max_size = sig_size + 4 + 4 + ((m-1)*getTreeHeight(n, m) + 1)*hash_size;
  //std::cerr << "max sig size " << max_size << std::endl;
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
    for (int j = ((n - 1 + (m - 2)) / (m - 1)) + i; j >= 1; j=(j-1) / m) {
      //std::cerr << "curr node " << j << std::endl;
      // append the next hash on the path to the root to the signature
      for (int k = 1; k <= m; ++k) {
        int l = (j-1)/m * m + k;
        if (l > max_leaf) {
          break;
        }
        //std::cerr << "checking sibling " << l << " " << h << std::endl;
        if (l != j) {
          //std::cerr << "copy hash " << l << " to " << starting_pos + h*hash_size << std::endl;
          memcpy(&sig[starting_pos + h*hash_size], &tree[l*hash_size], hash_size);
          h++;
        }
      }
    }
    // replace the sig with the raw signature bytes (performs a copy)
    sigs.emplace_back((char *) sig, starting_pos + h*hash_size);
    assert(sigs[i].size() == starting_pos + h*hash_size);
  }
}

}
