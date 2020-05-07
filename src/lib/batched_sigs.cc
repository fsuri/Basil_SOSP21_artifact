#include "lib/assert.h"
#include "lib/batched_sigs.h"
#include <cstring>
#include <unordered_map>

namespace BatchedSigs {

std::string packInt(unsigned int i) {
  unsigned char buf[4];
  buf[0] = ((unsigned int) i >> 24) & 0xFF;
  buf[1] = ((unsigned int) i >> 16) & 0xFF;
  buf[2] = ((unsigned int) i >> 8) & 0xFF;
  buf[3] = ((unsigned int) i) & 0xFF;
  return std::string(reinterpret_cast<char*>(buf), 4);
}

unsigned int unpackInt(std::string str) {
  unsigned int tmp = (unsigned char) str[0];
  tmp = (tmp << 8) | (unsigned char) str[1];
  tmp = (tmp << 8) | (unsigned char) str[2];
  tmp = (tmp << 8) | (unsigned char) str[3];
  return tmp;
}

std::string string_to_hex(const std::string& input)
{
    static const char hex_digits[] = "0123456789ABCDEF";

    std::string output;
    output.reserve(input.length() * 2);
    for (unsigned char c : input)
    {
        output.push_back(hex_digits[c >> 4]);
        output.push_back(hex_digits[c & 15]);
    }
    return output;
}

std::vector<std::string> generateBatchedSignatures(std::vector<std::string> messages, crypto::PrivKey* privateKey) {
  // generate the merkel tree
  unsigned int n = messages.size();
  assert(n > 0);

  std::vector<std::string> hashes;
  for (unsigned int i = 0; i < n-1; i++) {
    hashes.push_back("");
  }
  for (unsigned int i = 0; i < n; i++) {
    hashes.push_back(crypto::Hash(messages[i]));
  }
  // hashes now has 2n-1 elements
  assert(hashes.size() == 2*n - 1);

  for (int i = 2*n - 2; i >= 2; i-=2) {
    std::string concatHash = hashes[i-1]+ hashes[i];
    hashes[(i/2) - 1] = crypto::Hash(concatHash);
  }

  std::string rootSig = crypto::Sign(privateKey, hashes[0]);

  std::vector<std::string> sigs;
  size_t sig_size = crypto::SigSize(privateKey);

  for (unsigned int i = 0; i < n; i++) {
    std::string hash = crypto::Hash(messages[i]);
    std::string sig = rootSig + packInt(n) + packInt(i);
    for (int j = n - 1 + i; j >= 1; j=(j+1)/2 - 1) {
      assert(hash == hashes[j]);
      sig += j % 2 == 0 ? hashes[j - 1] : hashes[j + 1];
      hash = crypto::Hash(j % 2 == 0 ? hashes[j-1] + hash : hash + hashes[j+1]);
    }
    sigs.push_back(sig);
  }

  return sigs;
}

std::unordered_map<std::string, std::string> cache;

bool verifyBatchedSignature(std::string signature, std::string message, crypto::PubKey* publicKey) {
  size_t hash_size = crypto::HashSize();
  size_t sig_size = crypto::SigSize(publicKey);
  std::string rootSig = signature.substr(0, sig_size);
  unsigned int n = unpackInt(signature.substr(sig_size, 4));
  unsigned int i = unpackInt(signature.substr(sig_size+4, 4));
  int starting_pos = sig_size + 8;

  std::string hash = crypto::Hash(message);
  int h = 0;
  for (int j = n - 1 + i; j >= 1; j=(j+1)/2 - 1) {
    std::string nextHash = signature.substr(starting_pos + h*hash_size, hash_size);
    hash = crypto::Hash(j % 2 == 0 ? nextHash + hash : hash + nextHash);
    h++;
  }

  if (cache.find(rootSig) != cache.end()) {
    return cache[rootSig] == hash;
  } else {
    if (crypto::Verify(publicKey, hash, rootSig)) {
      cache[rootSig] = hash;
      return true;
    } else {
      return false;
    }
  }
}

}
