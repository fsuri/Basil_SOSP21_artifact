#ifndef _LIB_BATCHED_SIGS_H_
#define _LIB_BATCHED_SIGS_H_

#include <vector>
#include <string>
#include <cstring>
#include "lib/crypto.h"
#include "lib/blake3.h"

#include <iostream>



namespace BatchedSigs {

void packInt(unsigned int i, unsigned char* out);
unsigned int unpackInt(unsigned char* in);
void bhash_cat(unsigned char* in1, unsigned char* in2, unsigned char* out);
void bhash(unsigned char* in, size_t len, unsigned char* out);

void generateBatchedSignatures(const std::vector<const std::string*> &messages, crypto::PrivKey* privateKey, std::vector<std::string> &sigs, uint64_t m = 2);

extern thread_local uint64_t hashCount;
extern thread_local uint64_t hashCatCount;
extern thread_local blake3_hasher hasher;

template<class S>
bool computeBatchedSignatureHash(const std::string* signature, const std::string* message, crypto::PubKey* publicKey,
    S &hashStr, S &rootSig, uint64_t m = 2) {
  size_t hash_size = BLAKE3_OUT_LEN;
  size_t sig_size = crypto::SigSize(publicKey);

  // Debug("Computed hashStr pointer: %p", &hashStr);
  // Debug("Computed rootSig pointer: %p", &rootSig);
  // get the signature of the root, the number of signatures in the batch (n)
  // and this message's index in the batch (i)
  rootSig.resize(sig_size);
  rootSig.replace(rootSig.begin(), rootSig.end(), &(*signature)[0], &(*signature)[sig_size]);
  unsigned int n = unpackInt((unsigned char*) &signature->at(sig_size));
  unsigned int i = unpackInt((unsigned char*) &signature->at(sig_size+4));
  // compute the position where the hashes start in the signature
  unsigned int starting_pos = sig_size + 8;

  // this will store the hash as we traverse the merkle tree
  unsigned char hash[BLAKE3_OUT_LEN];
  // the leaf hash is the hash of the message
  bhash((unsigned char*) &message->at(0), message->length(), &hash[0]);
  hashCount++;
  // h is the index of the sibling hash in the signature
  int h = 0;
  // j is the current position in the tree.
  // invariant: [hash] is the hash of node j in the merkle tree
  int max_leaf = ((n - 1 + (m - 2)) / (m - 1) + n - 1);
  //std::cerr << "max leaf " << max_leaf << std::endl;
  for (int j = ((n - 1 + (m - 2)) / (m - 1)) + i; j >= 1; j=(j-1) / m) {
    //std::cerr << "curr node " << j << std::endl;
    // append the next hash on the path to the root to the signature
    blake3_hasher_init(&hasher);

    int leftmost_sib = (j-1)/m * m + 1;
    int rightmost_sib = leftmost_sib + m - 1;
    if (rightmost_sib > max_leaf) {
      rightmost_sib = max_leaf;
    }
    //std::cerr << "ls " << leftmost_sib << " rs " << rightmost_sib << std::endl;
    if (memcmp(&hash[0], &signature->at(starting_pos + (h + (j - leftmost_sib)) * hash_size), hash_size) != 0) {
      return false;
    }
    blake3_hasher_update(&hasher, &signature->at(starting_pos + h * hash_size),
        ((rightmost_sib - leftmost_sib) + 1) * hash_size);
    h += (rightmost_sib - leftmost_sib) + 1;
    blake3_hasher_finalize(&hasher, hash, BLAKE3_OUT_LEN);
  }

  hashStr.resize(hash_size);
  hashStr.replace(hashStr.begin(), hashStr.end(), &hash[0], &hash[hash_size]);

  return true;
}

template<class S>
bool computeBatchedSignatureHash2(const std::string signature, const std::string message, crypto::PubKey* publicKey,
    std::string *hashStr, std::string *rootSig, uint64_t m = 2) {
  size_t hash_size = BLAKE3_OUT_LEN;
  size_t sig_size = crypto::SigSize(publicKey);

  // Debug("Computed hashStr pointer: %p", &hashStr);
  // Debug("Computed rootSig pointer: %p", &rootSig);
  // get the signature of the root, the number of signatures in the batch (n)
  // and this message's index in the batch (i)
  rootSig->resize(sig_size);
  rootSig->replace(rootSig->begin(), rootSig->end(), &(signature)[0], &(signature)[sig_size]);
  unsigned int n = unpackInt((unsigned char*) &signature.at(sig_size));
  unsigned int i = unpackInt((unsigned char*) &signature.at(sig_size+4));
  // compute the position where the hashes start in the signature
  unsigned int starting_pos = sig_size + 8;

  // this will store the hash as we traverse the merkle tree
  unsigned char hash[BLAKE3_OUT_LEN];
  // the leaf hash is the hash of the message
  bhash((unsigned char*) &message.at(0), message.length(), &hash[0]);
  hashCount++;
  // h is the index of the sibling hash in the signature
  int h = 0;
  // j is the current position in the tree.
  // invariant: [hash] is the hash of node j in the merkle tree
  int max_leaf = ((n - 1 + (m - 2)) / (m - 1) + n - 1);
  //std::cerr << "max leaf " << max_leaf << std::endl;
  for (int j = ((n - 1 + (m - 2)) / (m - 1)) + i; j >= 1; j=(j-1) / m) {
    //std::cerr << "curr node " << j << std::endl;
    // append the next hash on the path to the root to the signature
    blake3_hasher_init(&hasher);

    int leftmost_sib = (j-1)/m * m + 1;
    int rightmost_sib = leftmost_sib + m - 1;
    if (rightmost_sib > max_leaf) {
      rightmost_sib = max_leaf;
    }
    //std::cerr << "ls " << leftmost_sib << " rs " << rightmost_sib << std::endl;
    if (memcmp(&hash[0], &signature.at(starting_pos + (h + (j - leftmost_sib)) * hash_size), hash_size) != 0) {
      return false;
    }
    blake3_hasher_update(&hasher, &signature.at(starting_pos + h * hash_size),
        ((rightmost_sib - leftmost_sib) + 1) * hash_size);
    h += (rightmost_sib - leftmost_sib) + 1;
    blake3_hasher_finalize(&hasher, hash, BLAKE3_OUT_LEN);
  }

  hashStr->resize(hash_size);
  hashStr->replace(hashStr->begin(), hashStr->end(), &hash[0], &hash[hash_size]);

  return true;
}

}

#endif
