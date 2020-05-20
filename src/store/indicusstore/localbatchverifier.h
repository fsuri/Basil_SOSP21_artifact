#ifndef LOCAL_BATCH_VERIFIER_H
#define LOCAL_BATCH_VERIFIER_H

#include "store/indicusstore/verifier.h"
#include "store/indicusstore/localbatchverifier.h"
#include "store/common/stats.h"

#include <string>
#include <unordered_map>

namespace indicusstore {

class LocalBatchVerifier : public Verifier {
 public:
  LocalBatchVerifier(Stats &stats);
  virtual ~LocalBatchVerifier();

  virtual bool Verify(crypto::PubKey *publicKey, const std::string &message,
      const std::string &signature) override;

 private:
  Stats &stats;
  std::unordered_map<std::string, std::string> cache;

};

} // namespace indicusstore

#endif /* LOCAL_BATCH_VERIFIER_H */
