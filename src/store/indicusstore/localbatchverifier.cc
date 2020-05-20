#include "store/indicusstore/localbatchverifier.h"

#include "lib/crypto.h"
#include "lib/batched_sigs.h"
#include "store/indicusstore/common.h"
#include "lib/message.h"

namespace indicusstore {

LocalBatchVerifier::LocalBatchVerifier(Stats &stats) : stats(stats) {
}

LocalBatchVerifier::~LocalBatchVerifier() {
}

bool LocalBatchVerifier::Verify(crypto::PubKey *publicKey, const std::string &message,
    const std::string &signature) {
  std::string hashStr;
  std::string rootSig;
  BatchedSigs::computeBatchedSignatureHash(&signature, &message, publicKey,
      hashStr, rootSig);
  Debug("Root hash %s %s.", BytesToHex(hashStr, 100).c_str(),
      BytesToHex(rootSig, 100).c_str());
  auto itr = cache.find(rootSig);
  if (itr == cache.end()) {
    stats.Increment("verify_cache_miss");
    if (crypto::Verify(publicKey, hashStr, rootSig)) {
      cache[rootSig] = hashStr;
      return true;
    } else {
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
