#ifndef _LIB_BATCHED_SIGS_H_
#define _LIB_BATCHED_SIGS_H_

#include <vector>
#include <string>
#include "lib/crypto.h"
#include "lib/blake3.h"

namespace BatchedSigs {

void packInt(unsigned int i, unsigned char* out);
unsigned int unpackInt(unsigned char* in);
void bhash_cat(unsigned char* in1, unsigned char* in2, unsigned char* out);
void bhash(unsigned char* in, size_t len, unsigned char* out);

void generateBatchedSignatures(const std::vector<const std::string*> &messages, crypto::PrivKey* privateKey, std::vector<std::string> &sigs);

template<class S>
void computeBatchedSignatureHash(const std::string* signature, const std::string* message, crypto::PubKey* publicKey,
    S &hashStr, S &rootSig) {
  size_t hash_size = BLAKE3_OUT_LEN;
  size_t sig_size = crypto::SigSize(publicKey);

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

  hashStr.resize(hash_size);
  hashStr.replace(hashStr.begin(), hashStr.end(), &hash[0], &hash[hash_size]);
}

}

#endif
