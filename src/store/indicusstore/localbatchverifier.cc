#include "store/indicusstore/localbatchverifier.h"

#include "lib/crypto.h"
#include "lib/batched_sigs.h"
#include "store/indicusstore/common.h"
#include "lib/message.h"

namespace indicusstore {

LocalBatchVerifier::LocalBatchVerifier(uint64_t merkleBranchFactor, Stats &stats) :
  merkleBranchFactor(merkleBranchFactor), stats(stats) {
  _Latency_Init(&hashLat, "hash");
  _Latency_Init(&cryptoLat, "crypto");
}

LocalBatchVerifier::~LocalBatchVerifier() {
  Latency_Dump(&hashLat);
  Latency_Dump(&cryptoLat);
}

bool LocalBatchVerifier::Verify(crypto::PubKey *publicKey, const std::string &message,
    const std::string &signature) {
  std::string hashStr;
  std::string rootSig;
  Latency_Start(&hashLat);
  if (!BatchedSigs::computeBatchedSignatureHash(&signature, &message, publicKey,
      hashStr, rootSig, merkleBranchFactor)) {
    return false;
  }
  Latency_End(&hashLat);
  auto itr = cache.find(rootSig);
  if (itr == cache.end()) {
    stats.Increment("verify_cache_miss");
    Latency_Start(&cryptoLat);
    if (crypto::Verify(publicKey, &hashStr[0], hashStr.length(), &rootSig[0])) {
      Latency_End(&cryptoLat);
      cache[rootSig] = hashStr;
      return true;
    } else {
      Latency_End(&cryptoLat);
      Debug("Verification with public key failed.");
      return false;
    }
  } else {
    if (hashStr == itr->second) {
      stats.Increment("verify_cache_hit");
      return true;
    } else {
      Debug("Verification via cached hash %s failed.",
          BytesToHex(itr->second, 100).c_str());
      return false;
    }
  }
}

} // namespace indicusstore
